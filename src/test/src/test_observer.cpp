#include "test_common.h"
#include <iterator>

#if GAIA_OBSERVERS_ENABLED

TEST_CASE("Observer - simple") {
	TestWorld twld;
	uint32_t cnt = 0;
	bool isDel = false;

	const auto on_add = wld.observer() //
													.event(ecs::ObserverEvent::OnAdd)
													.all<Position>()
													.all<Acceleration>()
													.on_each([&cnt, &isDel]() {
														++cnt;
														isDel = false;
													})
													.entity();
	(void)on_add;
	const auto on_del = wld.observer() //
													.event(ecs::ObserverEvent::OnDel)
													.no<Position>()
													.no<Acceleration>()
													.on_each([&cnt, &isDel]() {
														++cnt;
														isDel = true;
													})
													.entity();
	(void)on_del;

	ecs::Entity e, e1, e2;

	// OnAdd
	{
		cnt = 0;
		e = wld.add();
		{
			// Observer will not trigger yet
			wld.add<Position>(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Both components were added, trigger the observer now.
			wld.add<Acceleration>(e);
			CHECK(cnt == 1);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			e1 = wld.copy(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			e2 = wld.copy_ext(e);
			CHECK(cnt == 1);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			ecs::Entity e3 = wld.add();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			ecs::EntityBuilder builder = wld.build(e3);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			builder.add<Acceleration>().add<Position>();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Commit changes. The observer will triggers now.
			builder.commit();
			CHECK(cnt == 1);
			CHECK_FALSE(isDel);
		}
	}

	// OnAdd disabled
	{
		wld.enable(on_add, false);

		cnt = 0;
		e = wld.add();
		{
			// Observer will not trigger yet
			wld.add<Position>(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Both components were added, trigger the observer now.
			wld.add<Acceleration>(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			e1 = wld.copy(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			e2 = wld.copy_ext(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		{
			cnt = 0;
			ecs::Entity e3 = wld.add();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			ecs::EntityBuilder builder = wld.build(e3);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			builder.add<Acceleration>().add<Position>();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Commit changes. The observer will triggers now.
			builder.commit();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
		}

		wld.enable(on_add, true);
	}

	// OnDel
	{
		cnt = 0;
		{
			// Observer will not trigger yet
			wld.del<Position>(e);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Both components were added, trigger the observer now.
			wld.del<Acceleration>(e);
			CHECK(cnt == 1);
			CHECK(isDel);
		}

		{
			cnt = 0;
			isDel = false;
			ecs::EntityBuilder builder = wld.build(e1);
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			builder.del<Acceleration>().del<Position>();
			CHECK(cnt == 0);
			CHECK_FALSE(isDel);
			// Commit changes. The observer will triggers now.
			builder.commit();
			CHECK(cnt == 1);
			CHECK(isDel);
		}
	}
}

TEST_CASE("Observer - builder exposes kind and scope") {
	TestWorld twld;

	const auto observerEntity = wld.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.kind(ecs::QueryCacheKind::All)
																	.scope(ecs::QueryCacheScope::Shared)
																	.all<Position>()
																	.on_each([](ecs::Iter&) {})
																	.entity();

	auto ss = wld.acc(observerEntity);
	const auto& obsHdr = ss.get<ecs::Observer_>();
	(void)obsHdr;
	const auto& obsData = wld.observers().data(observerEntity);
	CHECK(obsData.query.kind() == ecs::QueryCacheKind::All);
	CHECK(obsData.query.scope() == ecs::QueryCacheScope::Shared);
}

TEST_CASE("Observer - OnSet") {
	TestWorld twld;

	uint32_t hits = 0;
	ecs::Entity last = ecs::EntityBad;
	Position lastPos{};
	uint32_t sparseHits = 0;
	ecs::Entity sparseLast = ecs::EntityBad;
	PositionSparse lastSparsePos{};

	const auto observerSet = wld.observer()
															 .event(ecs::ObserverEvent::OnSet)
															 .all<Position>()
															 .on_each([&](ecs::Entity entity, const Position& pos) {
																 ++hits;
																 last = entity;
																 lastPos = pos;
															 })
															 .entity();
	(void)observerSet;

	const auto sparseObserverSet = wld.observer()
																		 .event(ecs::ObserverEvent::OnSet)
																		 .all<PositionSparse>()
																		 .on_each([&](ecs::Entity entity, const PositionSparse& pos) {
																			 ++sparseHits;
																			 sparseLast = entity;
																			 lastSparsePos = pos;
																		 })
																		 .entity();
	(void)sparseObserverSet;

	const auto e = wld.add();
	wld.add<Position>(e);
	wld.add<Rotation>(e, {4.0f, 5.0f, 6.0f});

	CHECK(hits == 0);

	wld.set<Rotation>(e) = {7.0f, 8.0f, 9.0f};
	CHECK(hits == 0);

	wld.set<Position>(e) = {10.0f, 11.0f, 12.0f};
	CHECK(hits == 1);
	CHECK(last == e);
	CHECK(lastPos.x == doctest::Approx(10.0f));
	CHECK(lastPos.y == doctest::Approx(11.0f));
	CHECK(lastPos.z == doctest::Approx(12.0f));

	wld.acc_mut(e).set<Position>({13.0f, 14.0f, 15.0f});
	CHECK(hits == 2);
	CHECK(last == e);
	CHECK(lastPos.x == doctest::Approx(13.0f));
	CHECK(lastPos.y == doctest::Approx(14.0f));
	CHECK(lastPos.z == doctest::Approx(15.0f));

	wld.acc_mut(e).sset<Position>({16.0f, 17.0f, 18.0f});
	CHECK(hits == 2);

	wld.modify<Position, false>(e);
	CHECK(hits == 2);

	wld.modify<Position, true>(e);
	CHECK(hits == 3);
	CHECK(last == e);

	wld.add<PositionSparse>(e);
	CHECK(sparseHits == 0);

	wld.set<PositionSparse>(e) = {19.0f, 20.0f, 21.0f};
	CHECK(sparseHits == 1);
	CHECK(sparseLast == e);
	CHECK(lastSparsePos.x == doctest::Approx(19.0f));
	CHECK(lastSparsePos.y == doctest::Approx(20.0f));
	CHECK(lastSparsePos.z == doctest::Approx(21.0f));

	const auto e2 = wld.add();
	wld.add<Position>(e2, {22.0f, 23.0f, 24.0f});
	CHECK(hits == 3);
	CHECK(last == e);

	wld.set<Position>(e2) = {25.0f, 26.0f, 27.0f};
	CHECK(hits == 4);
	CHECK(last == e2);
	CHECK(lastPos.x == doctest::Approx(25.0f));
	CHECK(lastPos.y == doctest::Approx(26.0f));
	CHECK(lastPos.z == doctest::Approx(27.0f));
}

TEST_CASE("World modify emits OnSet for raw writes") {
	TestWorld twld;

	uint32_t hits = 0;
	Position lastPos{};
	uint32_t sparseHits = 0;
	PositionSparse lastSparsePos{};

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnSet)
			.all<Position>()
			.on_each([&](ecs::Entity, const Position& pos) {
				++hits;
				lastPos = pos;
			})
			.entity();

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnSet)
			.all<PositionSparse>()
			.on_each([&](ecs::Entity, const PositionSparse& pos) {
				++sparseHits;
				lastSparsePos = pos;
			})
			.entity();

	const auto e = wld.add();
	wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
	wld.add<PositionSparse>(e, {4.0f, 5.0f, 6.0f});

	{
		auto& pos = wld.mut<Position>(e);
		pos = {7.0f, 8.0f, 9.0f};
	}
	wld.modify<Position, false>(e);
	CHECK(hits == 0);
	wld.modify<Position, true>(e);
	CHECK(hits == 1);
	CHECK(lastPos.x == doctest::Approx(7.0f));
	CHECK(lastPos.y == doctest::Approx(8.0f));
	CHECK(lastPos.z == doctest::Approx(9.0f));

	{
		auto& pos = wld.mut<PositionSparse>(e);
		pos = {10.0f, 11.0f, 12.0f};
	}
	wld.modify<PositionSparse, false>(e);
	CHECK(sparseHits == 0);
	wld.modify<PositionSparse, true>(e);
	CHECK(sparseHits == 1);
	CHECK(lastSparsePos.x == doctest::Approx(10.0f));
	CHECK(lastSparsePos.y == doctest::Approx(11.0f));
	CHECK(lastSparsePos.z == doctest::Approx(12.0f));
}

TEST_CASE("World modify emits OnSet for raw object writes") {
	TestWorld twld;

	const auto& runtimeTableComp = wld.add(
			"Observer_Runtime_Table_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	const auto& runtimeSparseComp = wld.add(
			"Observer_Runtime_Sparse_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeSparseComp.entity, ecs::DontFragment);

	uint32_t tableHits = 0;
	ecs::Entity lastTable = ecs::EntityBad;
	uint32_t sparseHits = 0;
	ecs::Entity lastSparse = ecs::EntityBad;

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnSet)
			.all(runtimeTableComp.entity)
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++tableHits;
					lastTable = entities[i];
				}
			})
			.entity();

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnSet)
			.all(runtimeSparseComp.entity)
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++sparseHits;
					lastSparse = entities[i];
				}
			})
			.entity();

	const auto e = wld.add();
	wld.add(e, runtimeTableComp.entity, Position{1.0f, 2.0f, 3.0f});
	wld.add(e, runtimeSparseComp.entity, Position{4.0f, 5.0f, 6.0f});

	{
		auto& pos = wld.mut<Position>(e, runtimeTableComp.entity);
		pos = {7.0f, 8.0f, 9.0f};
	}
	wld.modify<Position, false>(e, runtimeTableComp.entity);
	CHECK(tableHits == 0);
	wld.modify<Position, true>(e, runtimeTableComp.entity);
	CHECK(tableHits == 1);
	CHECK(lastTable == e);
	{
		const auto& pos = wld.get<Position>(e, runtimeTableComp.entity);
		CHECK(pos.x == doctest::Approx(7.0f));
		CHECK(pos.y == doctest::Approx(8.0f));
		CHECK(pos.z == doctest::Approx(9.0f));
	}

	{
		auto& pos = wld.mut<Position>(e, runtimeSparseComp.entity);
		pos = {10.0f, 11.0f, 12.0f};
	}
	wld.modify<Position, false>(e, runtimeSparseComp.entity);
	CHECK(sparseHits == 0);
	wld.modify<Position, true>(e, runtimeSparseComp.entity);
	CHECK(sparseHits == 1);
	CHECK(lastSparse == e);
	{
		const auto& pos = wld.get<Position>(e, runtimeSparseComp.entity);
		CHECK(pos.x == doctest::Approx(10.0f));
		CHECK(pos.y == doctest::Approx(11.0f));
		CHECK(pos.z == doctest::Approx(12.0f));
	}
}

TEST_CASE("Observer - OnSet for immediate object writes") {
	TestWorld twld;

	const auto& runtimeTableComp = wld.add(
			"Observer_Runtime_Table_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	const auto& runtimeSparseComp = wld.add(
			"Observer_Runtime_Sparse_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeSparseComp.entity, ecs::DontFragment);

	uint32_t tableHits = 0;
	ecs::Entity lastTable = ecs::EntityBad;
	uint32_t sparseHits = 0;
	ecs::Entity lastSparse = ecs::EntityBad;

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnSet)
			.all(runtimeTableComp.entity)
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++tableHits;
					lastTable = entities[i];
				}
			})
			.entity();

	(void)wld.observer()
			.event(ecs::ObserverEvent::OnSet)
			.all(runtimeSparseComp.entity)
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++sparseHits;
					lastSparse = entities[i];
				}
			})
			.entity();

	const auto e = wld.add();
	wld.add(e, runtimeTableComp.entity, Position{1.0f, 2.0f, 3.0f});
	wld.add(e, runtimeSparseComp.entity, Position{4.0f, 5.0f, 6.0f});

	auto setter = wld.acc_mut(e);
	setter.set<Position>(runtimeTableComp.entity, Position{7.0f, 8.0f, 9.0f});
	CHECK(tableHits == 1);
	CHECK(lastTable == e);

	setter.sset<Position>(runtimeTableComp.entity, Position{10.0f, 11.0f, 12.0f});
	CHECK(tableHits == 1);

	setter.set<Position>(runtimeSparseComp.entity, Position{13.0f, 14.0f, 15.0f});
	CHECK(sparseHits == 1);
	CHECK(lastSparse == e);

	setter.sset<Position>(runtimeSparseComp.entity, Position{16.0f, 17.0f, 18.0f});
	CHECK(sparseHits == 1);
}

TEST_CASE("Observer - OnSet after query write callbacks") {
	SUBCASE("typed chunk-backed query") {
		TestWorld twld;

		uint32_t hits = 0;
		Position lastPos{};
		const auto observer = wld.observer()
															.event(ecs::ObserverEvent::OnSet)
															.all<Position>()
															.on_each([&](const Position& pos) {
																++hits;
																lastPos = pos;
															})
															.entity();
		(void)observer;

		const auto e = wld.add();
		wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
		hits = 0;

		wld.query().all<Position&>().each([&](Position& pos) {
			CHECK(hits == 0);
			pos.x = 10.0f;
			pos.y = 11.0f;
			pos.z = 12.0f;
			CHECK(hits == 0);
		});

		CHECK(hits == 1);
		CHECK(lastPos.x == doctest::Approx(10.0f));
		CHECK(lastPos.y == doctest::Approx(11.0f));
		CHECK(lastPos.z == doctest::Approx(12.0f));
	}

	SUBCASE("Iter chunk-backed query") {
		TestWorld twld;

		uint32_t hits = 0;
		Position lastPos{};
		const auto observer = wld.observer()
															.event(ecs::ObserverEvent::OnSet)
															.all<Position>()
															.on_each([&](const Position& pos) {
																++hits;
																lastPos = pos;
															})
															.entity();
		(void)observer;

		const auto e = wld.add();
		wld.add<Position>(e, {1.0f, 2.0f, 3.0f});
		hits = 0;

		wld.query().all<Position&>().each([&](ecs::Iter& it) {
			auto posView = it.view_mut<Position>();
			CHECK(hits == 0);
			GAIA_EACH(it) {
				posView[i].x = 20.0f;
				posView[i].y = 21.0f;
				posView[i].z = 22.0f;
				CHECK(hits == 0);
			}
		});

		CHECK(hits == 1);
		CHECK(lastPos.x == doctest::Approx(20.0f));
		CHECK(lastPos.y == doctest::Approx(21.0f));
		CHECK(lastPos.z == doctest::Approx(22.0f));
	}

	SUBCASE("typed sparse query") {
		TestWorld twld;

		uint32_t hits = 0;
		PositionSparse lastPos{};
		const auto observer = wld.observer()
															.event(ecs::ObserverEvent::OnSet)
															.all<PositionSparse>()
															.on_each([&](const PositionSparse& pos) {
																++hits;
																lastPos = pos;
															})
															.entity();
		(void)observer;

		const auto e = wld.add();
		wld.add<PositionSparse>(e, {1.0f, 2.0f, 3.0f});
		hits = 0;

		wld.query().all<PositionSparse&>().each([&](PositionSparse& pos) {
			CHECK(hits == 0);
			pos.x = 30.0f;
			pos.y = 31.0f;
			pos.z = 32.0f;
			CHECK(hits == 0);
		});

		CHECK(hits == 1);
		CHECK(lastPos.x == doctest::Approx(30.0f));
		CHECK(lastPos.y == doctest::Approx(31.0f));
		CHECK(lastPos.z == doctest::Approx(32.0f));
	}
}

TEST_CASE("Observer - write callbacks emit OnSet after callback completion") {
	SUBCASE("typed callback") {
		TestWorld twld;

		uint32_t onSetHits = 0;
		Position lastPos{};
		const auto onSetObserver = wld.observer()
																	 .event(ecs::ObserverEvent::OnSet)
																	 .all<Position>()
																	 .on_each([&](const Position& pos) {
																		 ++onSetHits;
																		 lastPos = pos;
																	 })
																	 .entity();
		(void)onSetObserver;

		const auto onAddObserver = wld.observer()
																	 .event(ecs::ObserverEvent::OnAdd)
																	 .all<Position&>()
																	 .on_each([&](Position& pos) {
																		 CHECK(onSetHits == 0);
																		 pos.x = 40.0f;
																		 pos.y = 41.0f;
																		 pos.z = 42.0f;
																		 CHECK(onSetHits == 0);
																	 })
																	 .entity();
		(void)onAddObserver;

		const auto e = wld.add();
		wld.add<Position>(e, {1.0f, 2.0f, 3.0f});

		CHECK(onSetHits == 1);
		CHECK(lastPos.x == doctest::Approx(40.0f));
		CHECK(lastPos.y == doctest::Approx(41.0f));
		CHECK(lastPos.z == doctest::Approx(42.0f));
	}

	SUBCASE("Iter callback") {
		TestWorld twld;

		uint32_t onSetHits = 0;
		Position lastPos{};
		const auto onSetObserver = wld.observer()
																	 .event(ecs::ObserverEvent::OnSet)
																	 .all<Position>()
																	 .on_each([&](const Position& pos) {
																		 ++onSetHits;
																		 lastPos = pos;
																	 })
																	 .entity();
		(void)onSetObserver;

		const auto onAddObserver = wld.observer()
																	 .event(ecs::ObserverEvent::OnAdd)
																	 .all<Position&>()
																	 .on_each([&](ecs::Iter& it) {
																		 auto posView = it.view_mut<Position>();
																		 CHECK(onSetHits == 0);
																		 GAIA_EACH(it) {
																			 posView[i].x = 50.0f;
																			 posView[i].y = 51.0f;
																			 posView[i].z = 52.0f;
																			 CHECK(onSetHits == 0);
																		 }
																	 })
																	 .entity();
		(void)onAddObserver;

		const auto e = wld.add();
		wld.add<Position>(e, {4.0f, 5.0f, 6.0f});

		CHECK(onSetHits == 1);
		CHECK(lastPos.x == doctest::Approx(50.0f));
		CHECK(lastPos.y == doctest::Approx(51.0f));
		CHECK(lastPos.z == doctest::Approx(52.0f));
	}
}

TEST_CASE("EntityBuilder single-step graph rebuild tolerates stale del edges") {
	TestWorld twld;

	auto A = wld.add();
	auto B = wld.add();
	auto C = wld.add();

	auto e = wld.add();
	wld.add(e, A);
	wld.add(e, B);
	wld.add(e, C);
	wld.del(A);

	for (int i = 0; i < 2000; ++i) {
		auto x = wld.add();
		wld.add(x, B);
		wld.add(x, C);
		wld.del(x, B);
		wld.del(x);
		wld.update();
	}
}

TEST_CASE("EntityBuilder batching and later single-step archetype moves") {
	TestWorld twld;

	const auto tagA = wld.add();
	const auto tagB = wld.add();
	const auto eBatch = wld.add();

	{
		auto builder = wld.build(eBatch);
		builder.add(tagA).add(tagB);
		builder.commit();
	}

	CHECK(wld.has(eBatch, tagA));
	CHECK(wld.has(eBatch, tagB));

	const auto eSingle = wld.add();
	wld.add(eSingle, tagA);
	wld.add(eSingle, tagB);
	CHECK(wld.has(eSingle, tagA));
	CHECK(wld.has(eSingle, tagB));
}

TEST_CASE("Observer - copy_ext payload") {
	TestWorld twld;

	uint32_t hits = 0;
	uint32_t iterSize = 0;
	uint32_t entityViewSize = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	Position pos{};
	Acceleration acc{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<Position>()
											 .all<Acceleration>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 iterSize = it.size();
												 auto entityView = it.view<ecs::Entity>();
												 entityViewSize = (uint32_t)entityView.size();
												 observedEntity = entityView[0];
												 auto posView = it.view<Position>();
												 auto accView = it.view<Acceleration>();
												 pos = posView[0];
												 acc = accView[0];
											 })
											 .entity();
	(void)obs;

	const auto src = wld.add();
	wld.add<Position>(src, {11.0f, 22.0f, 33.0f});
	wld.add<Acceleration>(src, {44.0f, 55.0f, 66.0f});

	hits = 0;
	iterSize = 0;
	entityViewSize = 0;
	observedEntity = ecs::EntityBad;
	pos = {};
	acc = {};

	const auto dst = wld.copy_ext(src);

	CHECK(hits == 1);
	CHECK(iterSize == 1);
	CHECK(entityViewSize == 1);
	CHECK(observedEntity == dst);
	CHECK(pos.x == 11.0f);
	CHECK(pos.y == 22.0f);
	CHECK(pos.z == 33.0f);
	CHECK(acc.x == 44.0f);
	CHECK(acc.y == 55.0f);
	CHECK(acc.z == 66.0f);
}

TEST_CASE("Observer - add with value payload") {
	TestWorld twld;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	Position pos{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<Position>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view<Position>();
												 observedEntity = entityView[0];
												 pos = posView[0];
											 })
											 .entity();
	(void)obs;

	const auto e = wld.add();
	wld.add(e, wld.add<Position>().entity, Position{3.0f, 4.0f, 5.0f});

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(3.0f));
	CHECK(pos.y == doctest::Approx(4.0f));
	CHECK(pos.z == doctest::Approx(5.0f));
}

TEST_CASE("Observer - add sparse with value payload") {
	TestWorld twld;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<PositionSparse>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view_any<PositionSparse>();
												 observedEntity = entityView[0];
												 pos = posView[0];
											 })
											 .entity();
	(void)obs;

	const auto e = wld.add();
	wld.add(e, wld.add<PositionSparse>().entity, PositionSparse{6.0f, 7.0f, 8.0f});

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(6.0f));
	CHECK(pos.y == doctest::Approx(7.0f));
	CHECK(pos.z == doctest::Approx(8.0f));
}

TEST_CASE("Observer - del sparse with value payload") {
	TestWorld twld;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnDel)
											 .all<PositionSparse>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view_any<PositionSparse>();
												 observedEntity = entityView[0];
												 pos = posView[0];
											 })
											 .entity();
	(void)obs;

	const auto e = wld.add();
	wld.add<PositionSparse>(e, {6.0f, 7.0f, 8.0f});

	hits = 0;
	observedEntity = ecs::EntityBad;
	pos = {};

	wld.del<PositionSparse>(e);

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(6.0f));
	CHECK(pos.y == doctest::Approx(7.0f));
	CHECK(pos.z == doctest::Approx(8.0f));
}

TEST_CASE("Observer - copy_ext sparse payload") {
	TestWorld twld;

	uint32_t hits = 0;
	uint32_t iterSize = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<PositionSparse>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 iterSize = it.size();
												 auto entityView = it.view<ecs::Entity>();
												 observedEntity = entityView[0];
												 auto posView = it.view_any<PositionSparse>();
												 pos = posView[0];
											 })
											 .entity();
	(void)obs;

	const auto src = wld.add();
	wld.add<PositionSparse>(src, {7.0f, 8.0f, 9.0f});

	hits = 0;
	iterSize = 0;
	observedEntity = ecs::EntityBad;
	pos = {};

	const auto dst = wld.copy_ext(src);

	CHECK(hits == 1);
	CHECK(iterSize == 1);
	CHECK(observedEntity == dst);
	CHECK(pos.x == 7.0f);
	CHECK(pos.y == 8.0f);
	CHECK(pos.z == 9.0f);
}

TEST_CASE("Observer - copy_ext_n payload") {
	TestWorld twld;

	constexpr uint32_t N = 1536;
	const auto src = wld.add();
	wld.add<Position>(src, {1.0f, 2.0f, 3.0f});
	wld.add<Acceleration>(src, {4.0f, 5.0f, 6.0f});

	uint32_t hits = 0;
	uint32_t seen = 0;
	cnt::darr<ecs::Entity> observedEntities;
	observedEntities.reserve(N);
	Position pos{};
	Acceleration acc{};

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<Position>()
											 .all<Acceleration>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 seen += it.size();
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view<Position>();
												 auto accView = it.view<Acceleration>();
												 GAIA_EACH(it) {
													 observedEntities.push_back(entityView[i]);
													 pos = posView[i];
													 acc = accView[i];
													 CHECK(pos.x == 1.0f);
													 CHECK(pos.y == 2.0f);
													 CHECK(pos.z == 3.0f);
													 CHECK(acc.x == 4.0f);
													 CHECK(acc.y == 5.0f);
													 CHECK(acc.z == 6.0f);
												 }
											 })
											 .entity();
	(void)obs;

	uint32_t callbackSeen = 0;
	wld.copy_ext_n(src, N, [&](ecs::CopyIter& it) {
		callbackSeen += it.size();
	});

	CHECK(hits >= 1);
	CHECK(seen == N);
	CHECK(callbackSeen == N);
	CHECK(observedEntities.size() == N);
}

TEST_CASE("Observer - copy_ext_n sparse payload") {
	TestWorld twld;

	constexpr uint32_t N = 1536;
	const auto src = wld.add();
	wld.add<PositionSparse>(src, {7.0f, 8.0f, 9.0f});

	uint32_t hits = 0;
	uint32_t seen = 0;
	cnt::darr<ecs::Entity> observedEntities;
	observedEntities.reserve(N);

	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all<PositionSparse>()
											 .on_each([&](ecs::Iter& it) {
												 ++hits;
												 seen += it.size();
												 auto entityView = it.view<ecs::Entity>();
												 auto posView = it.view_any<PositionSparse>();
												 GAIA_EACH(it) {
													 observedEntities.push_back(entityView[i]);
													 const auto pos = posView[i];
													 CHECK(pos.x == 7.0f);
													 CHECK(pos.y == 8.0f);
													 CHECK(pos.z == 9.0f);
												 }
											 })
											 .entity();
	(void)obs;

	uint32_t callbackSeen = 0;
	wld.copy_ext_n(src, N, [&](ecs::Entity entity) {
		++callbackSeen;
		CHECK(wld.has<PositionSparse>(entity));
		const auto& pos = wld.get<PositionSparse>(entity);
		CHECK(pos.x == 7.0f);
		CHECK(pos.y == 8.0f);
		CHECK(pos.z == 9.0f);
	});

	CHECK(hits >= 1);
	CHECK(seen == N);
	CHECK(callbackSeen == N);
	CHECK(observedEntities.size() == N);
}

TEST_CASE("Observer - copy_ext with unrelated traversed source observers") {
	TestWorld twld;

	const auto connectedToA = wld.add();
	const auto connectedToB = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto src = wld.add();
	wld.add<Position>(src);
	wld.add(src, ecs::Pair(connectedToA, child));

	int hitsA = 0;
	int hitsB = 0;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToA, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsA;
			});

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToB, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsB;
			});

	(void)wld.copy_ext(src);
	CHECK(hitsA == 1);
	CHECK(hitsB == 0);
}

TEST_CASE("copy_ext_n without names and with sparse data") {
	TestWorld twld;

	const auto src = wld.add();
	wld.name(src, "copy-ext-n-source");
	wld.add<PositionSparse>(src, {3.0f, 4.0f, 5.0f});

	cnt::darr<ecs::Entity> copied;
	wld.copy_ext_n(src, 8, [&](ecs::Entity entity) {
		copied.push_back(entity);
	});

	CHECK(copied.size() == 8);
	CHECK(wld.get("copy-ext-n-source") == src);
	CHECK(wld.name(src) == "copy-ext-n-source");

	for (const auto entity: copied) {
		CHECK(wld.name(entity).empty());
		CHECK(wld.has<PositionSparse>(entity));
		const auto& pos = wld.get<PositionSparse>(entity);
		CHECK(pos.x == doctest::Approx(3.0f));
		CHECK(pos.y == doctest::Approx(4.0f));
		CHECK(pos.z == doctest::Approx(5.0f));
	}
}

TEST_CASE("Observer - fast path") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto fixedPair = (ecs::Entity)ecs::Pair(relation, target);

	const auto baseEntity = wld.add();
	const auto inheritingEntity = wld.add();
	wld.as(inheritingEntity, baseEntity);
	const auto isPair = (ecs::Entity)ecs::Pair(ecs::Is, baseEntity);
	const auto wildcardPair = (ecs::Entity)ecs::Pair(relation, ecs::All);

	const auto observerAllPos =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>().on_each([](ecs::Iter&) {}).entity();
	const auto observerNoPos =
			wld.observer().event(ecs::ObserverEvent::OnDel).no<Position>().on_each([](ecs::Iter&) {}).entity();
	const auto observerPairFixed =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all(fixedPair).on_each([](ecs::Iter&) {}).entity();
	const auto observerPairIs =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all(isPair).on_each([](ecs::Iter&) {}).entity();
	const auto observerPairWildcard =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all(wildcardPair).on_each([](ecs::Iter&) {}).entity();
	ecs::QueryTermOptions readOpts;
	readOpts.read();
	const auto observerAllPosRead =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>(readOpts).on_each([](ecs::Iter&) {}).entity();
	ecs::QueryTermOptions writeOpts;
	writeOpts.write();
	const auto observerAllPosWrite =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>(writeOpts).on_each([](ecs::Iter&) {}).entity();
	ecs::QueryTermOptions travKindOnlyOpts;
	travKindOnlyOpts.trav_kind(ecs::QueryTravKind::Down);
	const auto observerAllPosTravKindOnly = wld.observer()
																							.event(ecs::ObserverEvent::OnAdd)
																							.all<Position>(travKindOnlyOpts)
																							.on_each([](ecs::Iter&) {})
																							.entity();
	ecs::QueryTermOptions travDepthOnlyOpts;
	travDepthOnlyOpts.trav_depth(2);
	const auto observerAllPosTravDepthOnly = wld.observer()
																							 .event(ecs::ObserverEvent::OnAdd)
																							 .all<Position>(travDepthOnlyOpts)
																							 .on_each([](ecs::Iter&) {})
																							 .entity();
	ecs::QueryTermOptions srcOpts;
	srcOpts.src(wld.add());
	const auto observerAllPosSrc =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>(srcOpts).on_each([](ecs::Iter&) {}).entity();
	ecs::QueryTermOptions travOpts;
	travOpts.trav(ecs::ChildOf);
	const auto observerAllPosTrav =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>(travOpts).on_each([](ecs::Iter&) {}).entity();
	const auto observerCascade = wld.observer()
																	 .event(ecs::ObserverEvent::OnAdd)
																	 .all<Position>()
																	 .depth_order(ecs::ChildOf)
																	 .on_each([](ecs::Iter&) {})
																	 .entity();

	const auto& dataAllPos = wld.observers().data(observerAllPos);
	const auto& dataNoPos = wld.observers().data(observerNoPos);
	const auto& dataPairFixed = wld.observers().data(observerPairFixed);
	const auto& dataPairIs = wld.observers().data(observerPairIs);
	const auto& dataPairWildcard = wld.observers().data(observerPairWildcard);
	const auto& dataAllPosRead = wld.observers().data(observerAllPosRead);
	const auto& dataAllPosWrite = wld.observers().data(observerAllPosWrite);
	const auto& dataAllPosTravKindOnly = wld.observers().data(observerAllPosTravKindOnly);
	const auto& dataAllPosTravDepthOnly = wld.observers().data(observerAllPosTravDepthOnly);
	const auto& dataAllPosSrc = wld.observers().data(observerAllPosSrc);
	const auto& dataAllPosTrav = wld.observers().data(observerAllPosTrav);
	auto& dataCascade = wld.observers().data(observerCascade);

	CHECK(dataAllPos.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);
	CHECK(dataNoPos.plan.fastPath == ecs::ObserverPlan::FastPath::SingleNegativeTerm);
	CHECK(dataPairFixed.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);
	CHECK(dataPairIs.plan.fastPath == ecs::ObserverPlan::FastPath::Disabled);
	CHECK(dataPairWildcard.plan.fastPath == ecs::ObserverPlan::FastPath::Disabled);
	CHECK(dataAllPosRead.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);
	CHECK(dataAllPosWrite.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);
	CHECK(dataAllPosTravKindOnly.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);
	CHECK(dataAllPosTravDepthOnly.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);
	CHECK(dataAllPosSrc.plan.fastPath == ecs::ObserverPlan::FastPath::Disabled);
	CHECK(dataAllPosTrav.plan.fastPath == ecs::ObserverPlan::FastPath::Disabled);
	CHECK(dataCascade.query.fetch().ctx().data.groupBy == ecs::ChildOf);
	CHECK(dataCascade.query.fetch().ctx().data.groupByFunc != nullptr);

	int pairHits = 0;
	const auto observerPairRuntime = wld.observer()
																			 .event(ecs::ObserverEvent::OnAdd)
																			 .all(fixedPair)
																			 .on_each([&pairHits](ecs::Iter&) {
																				 ++pairHits;
																			 })
																			 .entity();
	const auto e = wld.add();
	wld.add(e, ecs::Pair(relation, target));
	CHECK(pairHits == 1);
	(void)observerPairRuntime;
}

TEST_CASE("Observer - direct source term changes trigger OnAdd and OnDel") {
	TestWorld twld;

	const auto source = wld.add();
	const auto matched = wld.add();
	const auto unmatched = wld.add();
	wld.add<Position>(matched, {1.0f, 2.0f, 3.0f});
	wld.add<Rotation>(unmatched, {4.0f, 5.0f, 6.0f, 7.0f});

	uint32_t addHits = 0;
	uint32_t delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.all<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all<Position>()
			.all<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++delHits;
					removed.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(source, {8.0f, 9.0f, 10.0f});
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	if (added.size() == 1)
		CHECK(added[0] == matched);

	wld.del<Acceleration>(source);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	if (removed.size() == 1)
		CHECK(removed[0] == matched);
}

TEST_CASE("Observer - deleting a direct source emits OnDel") {
	TestWorld twld;

	const auto source = wld.add();
	wld.add<Acceleration>(source, {1.0f, 2.0f, 3.0f});

	const auto matched = wld.add();
	wld.add<Position>(matched, {4.0f, 5.0f, 6.0f});

	uint32_t hits = 0;
	cnt::darr<ecs::Entity> removed;
	wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all<Position>()
			.all<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++hits;
					removed.push_back(entities[i]);
				}
			});

	wld.del(source);
	CHECK(hits == 1);
	CHECK(removed.size() == 1);
	if (removed.size() == 1)
		CHECK(removed[0] == matched);
}

TEST_CASE("Observer - direct source ignores recycled source ids") {
	TestWorld twld;

	const auto source = wld.add();
	const auto matched = wld.add();
	wld.add<Position>(matched, {1.0f, 2.0f, 3.0f});

	uint32_t hits = 0;
	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.all<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++hits;
					added.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(source, {4.0f, 5.0f, 6.0f});
	CHECK(hits == 1);
	CHECK(added.size() == 1);
	if (added.size() == 1)
		CHECK(added[0] == matched);

	hits = 0;
	added.clear();

	wld.del(source);
	wld.update();

	ecs::Entity recycled = ecs::EntityBad;
	for (uint32_t i = 0; i < 256 && recycled == ecs::EntityBad; ++i) {
		const auto candidate = wld.add();
		if (candidate.id() == source.id())
			recycled = candidate;
	}
	CHECK(recycled != ecs::EntityBad);
	if (recycled != ecs::EntityBad) {
		CHECK(recycled.id() == source.id());
		CHECK(recycled.gen() != source.gen());
		wld.add<Acceleration>(recycled, {7.0f, 8.0f, 9.0f});
	}

	CHECK(hits == 0);
	CHECK(added.empty());
}

TEST_CASE("Observer - direct source or term changes trigger OnAdd and OnDel") {
	TestWorld twld;

	const auto source = wld.add();
	const auto matched = wld.add();
	wld.add<Position>(matched, {1.0f, 2.0f, 3.0f});

	uint32_t addHits = 0;
	uint32_t delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.all<Position>()
			.or_<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.all<Position>()
			.or_<Acceleration>(ecs::QueryTermOptions{}.src(source))
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++delHits;
					removed.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(source, {4.0f, 5.0f, 6.0f});
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	if (added.size() == 1)
		CHECK(added[0] == matched);

	wld.del<Acceleration>(source);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	if (removed.size() == 1)
		CHECK(removed[0] == matched);
}

TEST_CASE("Observer - identical traversed observers keep local cached query state by default") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	cnt::darray<ecs::Entity> observers;
	observers.reserve(200);

	const auto makeObserver = [&] {
		return wld.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([](ecs::Iter&) {})
				.entity();
	};

	const auto observer0 = makeObserver();
	const auto expectedHandle = ecs::QueryInfo::handle(wld.observers().data(observer0).query.fetch());
	observers.push_back(observer0);

	for (uint32_t i = 1; i < 200; ++i) {
		const auto observer = makeObserver();
		const auto handle = ecs::QueryInfo::handle(wld.observers().data(observer).query.fetch());
		CHECK(handle != expectedHandle);
		observers.push_back(observer);
	}

	CHECK(observers.size() == 200);
}

TEST_CASE("Observer - identical traversed observers delete cleanly") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);

	cnt::darr<ecs::Entity> observers;
	const auto makeObserver = [&](ecs::ObserverEvent event) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([](ecs::Iter&) {})
				.entity();
	};

	observers.push_back(makeObserver(ecs::ObserverEvent::OnAdd));
	observers.push_back(makeObserver(ecs::ObserverEvent::OnDel));
	observers.push_back(makeObserver(ecs::ObserverEvent::OnAdd));
	observers.push_back(makeObserver(ecs::ObserverEvent::OnDel));

	const auto positionTerm = wld.add<Position>().entity;
	const auto accelerationTerm = wld.add<Acceleration>().entity;
	CHECK(wld.observers().has_observers(positionTerm));
	CHECK(wld.observers().has_observers(accelerationTerm));

	wld.del(observers[0]);
	wld.del(observers[1]);
	CHECK(wld.observers().has_observers(positionTerm));
	CHECK(wld.observers().has_observers(accelerationTerm));

	wld.del(observers[2]);
	wld.del(observers[3]);
	CHECK_FALSE(wld.observers().has_observers(positionTerm));
	CHECK_FALSE(wld.observers().has_observers(accelerationTerm));
}

namespace {
	template <typename TQuery>
	cnt::darr<ecs::Entity> collect_sorted_entities(TQuery query) {
		cnt::darr<ecs::Entity> out;
		query.each([&](ecs::Entity entity) {
			out.push_back(entity);
		});
		std::sort(out.begin(), out.end(), [](ecs::Entity left, ecs::Entity right) {
			if (left.id() != right.id())
				return left.id() < right.id();
			return left.gen() < right.gen();
		});
		return out;
	}

	cnt::darr<ecs::Entity> sorted_entity_diff(const cnt::darr<ecs::Entity>& lhs, const cnt::darr<ecs::Entity>& rhs) {
		cnt::darr<ecs::Entity> out;
		out.reserve(lhs.size());
		std::set_difference(
				lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), std::back_inserter(out),
				[](ecs::Entity left, ecs::Entity right) {
					if (left.id() != right.id())
						return left.id() < right.id();
					return left.gen() < right.gen();
				});
		return out;
	}

	std::string entity_list_string(const cnt::darr<ecs::Entity>& entities) {
		std::string out = "[";
		for (uint32_t i = 0; i < entities.size(); ++i) {
			if (i != 0)
				out += ", ";
			out += std::to_string(entities[i].id());
			out += ":";
			out += std::to_string(entities[i].gen());
		}
		out += "]";
		return out;
	}

	ecs::Entity recycle_same_id(ecs::World& world, ecs::Entity stale, uint32_t maxAttempts = 256) {
		for (uint32_t i = 0; i < maxAttempts; ++i) {
			const auto candidate = world.add();
			if (candidate.id() == stale.id())
				return candidate;
		}

		return ecs::EntityBad;
	}

	template <typename MutateFunc>
	void expect_traversed_observer_changes(
			ecs::World& world, ecs::Entity bindingRelation, ecs::QueryTermOptions traversalOptions, MutateFunc&& mutate,
			ecs::ObserverPlan::ExecKind expectedExecKind = ecs::ObserverPlan::ExecKind::DiffPropagated) {
		auto termOptions = traversalOptions;
		termOptions.src(ecs::Var0);

		auto buildQuery = [&]() {
			return world.uquery()
					.template all<Position>()
					.all(ecs::Pair(bindingRelation, ecs::Var0))
					.template all<Acceleration>(termOptions);
		};

		auto added = std::make_shared<cnt::darr<ecs::Entity>>();
		auto removed = std::make_shared<cnt::darr<ecs::Entity>>();

		const auto addObserver = world.observer()
																 .event(ecs::ObserverEvent::OnAdd)
																 .template all<Position>()
																 .all(ecs::Pair(bindingRelation, ecs::Var0))
																 .template all<Acceleration>(termOptions)
																 .on_each([added](ecs::Iter& it) {
																	 auto entities = it.view<ecs::Entity>();
																	 GAIA_EACH(it) {
																		 added->push_back(entities[i]);
																	 }
																 })
																 .entity();
		const auto delObserver = world.observer()
																 .event(ecs::ObserverEvent::OnDel)
																 .template all<Position>()
																 .all(ecs::Pair(bindingRelation, ecs::Var0))
																 .template all<Acceleration>(termOptions)
																 .on_each([removed](ecs::Iter& it) {
																	 auto entities = it.view<ecs::Entity>();
																	 GAIA_EACH(it) {
																		 removed->push_back(entities[i]);
																	 }
																 })
																 .entity();

		const auto& addData = world.observers().data(addObserver);
		const auto& delData = world.observers().data(delObserver);
		CHECK(addData.plan.exec_kind() == expectedExecKind);
		CHECK(delData.plan.exec_kind() == expectedExecKind);

		const auto before = collect_sorted_entities(buildQuery());
		mutate();
		const auto after = collect_sorted_entities(buildQuery());

		std::sort(added->begin(), added->end(), [](ecs::Entity left, ecs::Entity right) {
			if (left.id() != right.id())
				return left.id() < right.id();
			return left.gen() < right.gen();
		});
		std::sort(removed->begin(), removed->end(), [](ecs::Entity left, ecs::Entity right) {
			if (left.id() != right.id())
				return left.id() < right.id();
			return left.gen() < right.gen();
		});

		const auto expectedAdded = sorted_entity_diff(after, before);
		const auto expectedRemoved = sorted_entity_diff(before, after);

		INFO("before=" << entity_list_string(before));
		INFO("after=" << entity_list_string(after));
		INFO("added=" << entity_list_string(*added));
		INFO("expectedAdded=" << entity_list_string(expectedAdded));
		INFO("removed=" << entity_list_string(*removed));
		INFO("expectedRemoved=" << entity_list_string(expectedRemoved));

		CHECK(*added == expectedAdded);
		CHECK(*removed == expectedRemoved);
		(void)addObserver;
		(void)delObserver;
	}
} // namespace

TEST_CASE("Observer - traversed source propagation on ancestor term changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.add<Acceleration>(root);
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cable);

	wld.del<Acceleration>(root);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source propagation on ancestor term changes and unrelated existing matches") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootHot = wld.add();
	const auto childHot = wld.add();
	wld.child(childHot, rootHot);

	const auto rootCold = wld.add();
	const auto childCold = wld.add();
	wld.child(childCold, rootCold);
	wld.add<Acceleration>(rootCold);

	const auto cableCold = wld.add();
	wld.add<Position>(cableCold);
	wld.add(cableCold, ecs::Pair(connectedTo, childCold));

	const auto cableHot = wld.add();
	wld.add<Position>(cableHot);
	wld.add(cableHot, ecs::Pair(connectedTo, childHot));

	int addHits = 0;
	cnt::darr<ecs::Entity> added;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(rootHot);
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cableHot);
}

TEST_CASE("Observer - traversed source propagation on unrelated ancestor term changes emits nothing") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootMatching = wld.add();
	const auto childMatching = wld.add();
	wld.child(childMatching, rootMatching);
	wld.add<Acceleration>(rootMatching);

	const auto cableMatching = wld.add();
	wld.add<Position>(cableMatching);
	wld.add(cableMatching, ecs::Pair(connectedTo, childMatching));

	const auto rootUnrelated = wld.add();
	const auto childUnrelated = wld.add();
	wld.child(childUnrelated, rootUnrelated);

	int addHits = 0;
	cnt::darr<ecs::Entity> added;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.add<Acceleration>(rootUnrelated);
	CHECK(addHits == 0);
	CHECK(added.empty());
}

TEST_CASE("Observer - traversed source propagation on source binding pair changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.add(cable, ecs::Pair(connectedTo, child));
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cable);

	wld.del(cable, ecs::Pair(connectedTo, child));
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source local pair change only diffs touched entity") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cableExisting = wld.add();
	wld.add<Position>(cableExisting);
	wld.add(cableExisting, ecs::Pair(connectedTo, child));

	const auto cableAdded = wld.add();
	wld.add<Position>(cableAdded);

	int addHits = 0;
	cnt::darr<ecs::Entity> added;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++addHits;
					added.push_back(entities[i]);
				}
			});

	wld.add(cableAdded, ecs::Pair(connectedTo, child));
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	CHECK(added[0] == cableAdded);

	int delHits = 0;
	cnt::darr<ecs::Entity> removed;
	wld.observer()
			.event(ecs::ObserverEvent::OnDel)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					++delHits;
					removed.push_back(entities[i]);
				}
			});

	wld.del(cableAdded, ecs::Pair(connectedTo, child));
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	CHECK(removed[0] == cableAdded);
}

TEST_CASE("Observer - traversed source propagation on relation edge changes") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));
	auto q = wld.query()
							 .template all<Position>()
							 .all(ecs::Pair(connectedTo, ecs::Var0))
							 .template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	CHECK(q.count() == 0);

	int addHits = 0;
	int delHits = 0;
	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;

	const auto makeObserver = [&](ecs::ObserverEvent event, int& hits, cnt::darr<ecs::Entity>& out) {
		return wld.observer()
				.event(event)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						++hits;
						out.push_back(entities[i]);
					}
				})
				.entity();
	};
	const auto buildQuery = [&] {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	};

	(void)makeObserver(ecs::ObserverEvent::OnAdd, addHits, added);
	(void)makeObserver(ecs::ObserverEvent::OnDel, delHits, removed);

	wld.child(child, root);
	CHECK(wld.target(child, ecs::ChildOf) == root);
	CHECK(buildQuery().count() == 1);
	CHECK(addHits == 1);
	CHECK(added.size() == 1);
	if (!added.empty())
		CHECK(added[0] == cable);

	wld.del(child, ecs::Pair(ecs::ChildOf, root));
	CHECK(wld.target(child, ecs::ChildOf) == ecs::EntityBad);
	CHECK(buildQuery().count() == 0);
	CHECK(delHits == 1);
	CHECK(removed.size() == 1);
	if (!removed.empty())
		CHECK(removed[0] == cable);
}

TEST_CASE("Observer - traversed source propagation and unrelated pair relation changes") {
	TestWorld twld;

	const auto connectedToA = wld.add();
	const auto connectedToB = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);

	int hitsA = 0;
	int hitsB = 0;

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToA, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsA;
			});

	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedToB, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter&) {
				++hitsB;
			});

	wld.add(cable, ecs::Pair(connectedToA, child));
	CHECK(hitsA == 1);
	CHECK(hitsB == 0);
}

TEST_CASE("Observer - ancestor component diffs with many descendants") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	const auto grandChild = wld.add();
	wld.child(childA, root);
	wld.child(childB, root);
	wld.child(grandChild, childA);

	const auto cableA = wld.add();
	const auto cableB = wld.add();
	const auto cableC = wld.add();
	wld.add<Position>(cableA);
	wld.add<Position>(cableB);
	wld.add<Position>(cableC);
	wld.add(cableA, ecs::Pair(connectedTo, childA));
	wld.add(cableB, ecs::Pair(connectedTo, childB));
	wld.add(cableC, ecs::Pair(connectedTo, grandChild));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.add<Acceleration>(root);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del<Acceleration>(root);
	});
}

TEST_CASE("Observer - self-up closer-match transitions") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.add<Acceleration>(child);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del<Acceleration>(root);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del<Acceleration>(child);
	});
}

TEST_CASE("Observer - bounded parent traversal") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto parent = wld.add();
	const auto leaf = wld.add();
	wld.child(parent, root);
	wld.child(leaf, parent);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_parent(), [&] {
		wld.add<Acceleration>(root);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_parent(), [&] {
		wld.add<Acceleration>(parent);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_parent(), [&] {
		wld.del<Acceleration>(parent);
	});
}

TEST_CASE("Observer - bounded self-parent transitions") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto parent = wld.add();
	const auto leaf = wld.add();
	wld.child(parent, root);
	wld.child(leaf, parent);
	wld.add<Acceleration>(leaf);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_self_parent(), [&] {
		wld.add<Acceleration>(parent);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_self_parent(), [&] {
		wld.del<Acceleration>(leaf);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav_self_parent(), [&] {
		wld.del<Acceleration>(parent);
	});
}

TEST_CASE("Observer - repeated reachability paths") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));
	wld.add(leaf, ecs::Pair(linkedTo, midB));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.add<Acceleration>(root);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.del<Acceleration>(root);
	});
}

TEST_CASE("Observer - duplicate-path relation edge additions") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add<Acceleration>(root);
	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.add(leaf, ecs::Pair(linkedTo, midB));
	});
}

TEST_CASE("Observer - duplicate-path relation edge removals with remaining path") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add<Acceleration>(root);
	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));
	wld.add(leaf, ecs::Pair(linkedTo, midB));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.del(leaf, ecs::Pair(linkedTo, midB));
	});
}

TEST_CASE("Observer - deleting one repeated-path intermediate keeps the match") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add<Acceleration>(root);
	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));
	wld.add(leaf, ecs::Pair(linkedTo, midB));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.del(midB);
	});
}

TEST_CASE("Observer - deleting the last repeated-path intermediate removes the match") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto linkedTo = wld.add();
	const auto root = wld.add();
	const auto midA = wld.add();
	const auto midB = wld.add();
	const auto leaf = wld.add();

	wld.add<Acceleration>(root);
	wld.add(midA, ecs::Pair(linkedTo, root));
	wld.add(midB, ecs::Pair(linkedTo, root));
	wld.add(leaf, ecs::Pair(linkedTo, midA));
	wld.add(leaf, ecs::Pair(linkedTo, midB));

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, leaf));

	wld.del(midB);
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(linkedTo), [&] {
		wld.del(midA);
	});
}

TEST_CASE("Observer - removing one of multiple matching binding pairs") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, root);
	wld.child(childB, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, childA));
	wld.add(cable, ecs::Pair(connectedTo, childB));

	expect_traversed_observer_changes(
			wld, connectedTo, ecs::QueryTermOptions{}.trav(),
			[&] {
				wld.del(cable, ecs::Pair(connectedTo, childB));
			},
			ecs::ObserverPlan::ExecKind::DiffPropagated);

	expect_traversed_observer_changes(
			wld, connectedTo, ecs::QueryTermOptions{}.trav(),
			[&] {
				wld.del(cable, ecs::Pair(connectedTo, childA));
			},
			ecs::ObserverPlan::ExecKind::DiffPropagated);
}

TEST_CASE("Observer - another bound source still matches") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, rootA);
	wld.child(childB, rootB);
	wld.add<Acceleration>(rootA);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, childA));
	wld.add(cable, ecs::Pair(connectedTo, childB));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.add<Acceleration>(rootB);
	});
	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del<Acceleration>(rootA);
	});
}

TEST_CASE("Observer - deleting a matching ancestor updates all descendants") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	const auto grandChild = wld.add();
	wld.child(childA, root);
	wld.child(childB, root);
	wld.child(grandChild, childA);
	wld.add<Acceleration>(root);

	const auto cableA = wld.add();
	const auto cableB = wld.add();
	const auto cableC = wld.add();
	wld.add<Position>(cableA);
	wld.add<Position>(cableB);
	wld.add<Position>(cableC);
	wld.add(cableA, ecs::Pair(connectedTo, childA));
	wld.add(cableB, ecs::Pair(connectedTo, childB));
	wld.add(cableC, ecs::Pair(connectedTo, grandChild));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(root);
	});
}

TEST_CASE("Observer - deleting one bound source with another still matching") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, rootA);
	wld.child(childB, rootB);
	wld.add<Acceleration>(rootA);
	wld.add<Acceleration>(rootB);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, childA));
	wld.add(cable, ecs::Pair(connectedTo, childB));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(childB);
	});
}

TEST_CASE("Observer - deleting the only bound source emits removal") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(child);
	});
}

TEST_CASE("Observer - recycled bound source does not resurrect traversed matches") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(child);
		wld.update();

		const auto recycled = recycle_same_id(wld, child);
		CHECK(recycled != ecs::EntityBad);
		if (recycled != ecs::EntityBad) {
			CHECK(recycled.id() == child.id());
			CHECK(recycled.gen() != child.gen());
			wld.child(recycled, root);
		}
	});
}

TEST_CASE("Observer - recycled ancestor does not resurrect traversed matches") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));

	expect_traversed_observer_changes(wld, connectedTo, ecs::QueryTermOptions{}.trav(), [&] {
		wld.del(root);
		wld.update();

		const auto recycled = recycle_same_id(wld, root);
		CHECK(recycled != ecs::EntityBad);
		if (recycled != ecs::EntityBad) {
			CHECK(recycled.id() == root.id());
			CHECK(recycled.gen() != root.gen());
			wld.add<Acceleration>(recycled);
		}
	});
}

TEST_CASE("Observer - unsupported traversal diff shape") {
	TestWorld twld;

	struct Shield {};

	const auto connectedTo = wld.add();
	const auto guardedBy = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	const auto guardian = wld.add();
	wld.child(child, root);
	wld.child(guardian, root);

	const auto cable = wld.add();
	wld.add<Position>(cable);
	wld.add(cable, ecs::Pair(connectedTo, child));
	wld.add(cable, ecs::Pair(guardedBy, guardian));

	auto buildQuery = [&]() {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.all(ecs::Pair(guardedBy, ecs::Var1))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.template all<Shield>(ecs::QueryTermOptions{}.src(ecs::Var1).trav_parent());
	};

	cnt::darr<ecs::Entity> added;
	cnt::darr<ecs::Entity> removed;
	const auto addObserver = wld.observer()
															 .event(ecs::ObserverEvent::OnAdd)
															 .template all<Position>()
															 .all(ecs::Pair(connectedTo, ecs::Var0))
															 .all(ecs::Pair(guardedBy, ecs::Var1))
															 .template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
															 .template all<Shield>(ecs::QueryTermOptions{}.src(ecs::Var1).trav_parent())
															 .on_each([&](ecs::Iter& it) {
																 auto entities = it.view<ecs::Entity>();
																 GAIA_EACH(it) {
																	 added.push_back(entities[i]);
																 }
															 })
															 .entity();
	const auto delObserver = wld.observer()
															 .event(ecs::ObserverEvent::OnDel)
															 .template all<Position>()
															 .all(ecs::Pair(connectedTo, ecs::Var0))
															 .all(ecs::Pair(guardedBy, ecs::Var1))
															 .template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
															 .template all<Shield>(ecs::QueryTermOptions{}.src(ecs::Var1).trav_parent())
															 .on_each([&](ecs::Iter& it) {
																 auto entities = it.view<ecs::Entity>();
																 GAIA_EACH(it) {
																	 removed.push_back(entities[i]);
																 }
															 })
															 .entity();

	CHECK(wld.observers().data(addObserver).plan.uses_fallback_diff_dispatch());
	CHECK(wld.observers().data(delObserver).plan.uses_fallback_diff_dispatch());

	const auto before = collect_sorted_entities(buildQuery());
	wld.add<Acceleration>(root);
	const auto middle = collect_sorted_entities(buildQuery());
	wld.add<Shield>(root);
	const auto after = collect_sorted_entities(buildQuery());

	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(removed.begin(), removed.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	const auto expectedAddedFirst = sorted_entity_diff(middle, before);
	const auto expectedAddedSecond = sorted_entity_diff(after, middle);
	cnt::darr<ecs::Entity> expectedAdded = expectedAddedFirst;
	for (auto entity: expectedAddedSecond)
		expectedAdded.push_back(entity);
	std::sort(expectedAdded.begin(), expectedAdded.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	CHECK(added == expectedAdded);
	CHECK(removed.empty());
}

TEST_CASE("Observer - traversed source copy_ext_n fires for all new entities") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto src = wld.add();
	wld.add<Position>(src);
	wld.add(src, ecs::Pair(connectedTo, child));

	auto buildQuery = [&]() {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	};

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	const auto before = collect_sorted_entities(buildQuery());

	cnt::darr<ecs::Entity> copied;
	wld.copy_ext_n(src, 257, [&](ecs::Entity entity) {
		copied.push_back(entity);
	});

	const auto after = collect_sorted_entities(buildQuery());
	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(copied.begin(), copied.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	const auto expectedAdded = sorted_entity_diff(after, before);
	CHECK(added == expectedAdded);
	CHECK(added == copied);
}

TEST_CASE("Observer - traversed source copy_ext_n with multiple matching binding pairs") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, rootA);
	wld.child(childB, rootB);
	wld.add<Acceleration>(rootA);
	wld.add<Acceleration>(rootB);

	const auto src = wld.add();
	wld.add<Position>(src);
	wld.add(src, ecs::Pair(connectedTo, childA));
	wld.add(src, ecs::Pair(connectedTo, childB));

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	cnt::darr<ecs::Entity> copied;
	wld.copy_ext_n(src, 193, [&](ecs::Entity entity) {
		copied.push_back(entity);
	});

	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(copied.begin(), copied.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	CHECK(added == copied);
}

TEST_CASE("Observer - traversed source add_n fires for all new entities") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto archetypeSeed = wld.add();
	wld.add<Position>(archetypeSeed);
	wld.add(archetypeSeed, ecs::Pair(connectedTo, child));

	auto buildQuery = [&]() {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav());
	};

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	const auto before = collect_sorted_entities(buildQuery());

	cnt::darr<ecs::Entity> created;
	wld.add_n(archetypeSeed, 257, [&](ecs::Entity entity) {
		created.push_back(entity);
	});

	const auto after = collect_sorted_entities(buildQuery());
	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(created.begin(), created.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	const auto expectedAdded = sorted_entity_diff(after, before);
	CHECK(added == expectedAdded);
	CHECK(added == created);
}

TEST_CASE("Observer - traversed source add_n with multiple matching binding pairs") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto rootA = wld.add();
	const auto rootB = wld.add();
	const auto childA = wld.add();
	const auto childB = wld.add();
	wld.child(childA, rootA);
	wld.child(childB, rootB);
	wld.add<Acceleration>(rootA);
	wld.add<Acceleration>(rootB);

	const auto archetypeSeed = wld.add();
	wld.add<Position>(archetypeSeed);
	wld.add(archetypeSeed, ecs::Pair(connectedTo, childA));
	wld.add(archetypeSeed, ecs::Pair(connectedTo, childB));

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(connectedTo, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	cnt::darr<ecs::Entity> created;
	wld.add_n(archetypeSeed, 193, [&](ecs::Entity entity) {
		created.push_back(entity);
	});

	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(created.begin(), created.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	CHECK(added == created);
}

TEST_CASE("Observer - parented instantiate_n traversed source fires for all new roots") {
	TestWorld twld;

	const auto scene = wld.add();
	wld.add<Acceleration>(scene);

	const auto animal = wld.add();
	wld.add<Position>(animal);

	auto buildQuery = [&]() {
		return wld.uquery()
				.template all<Position>()
				.all(ecs::Pair(ecs::Parent, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_self_parent());
	};

	cnt::darr<ecs::Entity> added;
	wld.observer()
			.event(ecs::ObserverEvent::OnAdd)
			.template all<Position>()
			.all(ecs::Pair(ecs::Parent, ecs::Var0))
			.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav_self_parent())
			.on_each([&](ecs::Iter& it) {
				auto entities = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					added.push_back(entities[i]);
				}
			});

	const auto before = collect_sorted_entities(buildQuery());

	cnt::darr<ecs::Entity> created;
	wld.instantiate_n(animal, scene, 129, [&](ecs::Entity entity) {
		created.push_back(entity);
	});

	const auto after = collect_sorted_entities(buildQuery());
	std::sort(added.begin(), added.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});
	std::sort(created.begin(), created.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	const auto expectedAdded = sorted_entity_diff(after, before);
	CHECK(added == expectedAdded);
	CHECK(created.size() == 129);
}

TEST_CASE("Observer - many identical traversed observers each fire once for batched copy_ext_n") {
	TestWorld twld;

	const auto connectedTo = wld.add();
	const auto root = wld.add();
	const auto child = wld.add();
	wld.child(child, root);
	wld.add<Acceleration>(root);

	const auto src = wld.add();
	wld.add<Position>(src);
	wld.add(src, ecs::Pair(connectedTo, child));

	struct ObsCapture {
		cnt::darr<ecs::Entity> hits;
	};

	cnt::darr<ObsCapture> captures(24);
	for (auto& capture: captures) {
		wld.observer()
				.event(ecs::ObserverEvent::OnAdd)
				.template all<Position>()
				.all(ecs::Pair(connectedTo, ecs::Var0))
				.template all<Acceleration>(ecs::QueryTermOptions{}.src(ecs::Var0).trav())
				.on_each([&capture](ecs::Iter& it) {
					auto entities = it.view<ecs::Entity>();
					GAIA_EACH(it) {
						capture.hits.push_back(entities[i]);
					}
				});
	}

	cnt::darr<ecs::Entity> copied;
	wld.copy_ext_n(src, 97, [&](ecs::Entity entity) {
		copied.push_back(entity);
	});
	std::sort(copied.begin(), copied.end(), [](ecs::Entity left, ecs::Entity right) {
		if (left.id() != right.id())
			return left.id() < right.id();
		return left.gen() < right.gen();
	});

	for (auto& capture: captures) {
		std::sort(capture.hits.begin(), capture.hits.end(), [](ecs::Entity left, ecs::Entity right) {
			if (left.id() != right.id())
				return left.id() < right.id();
			return left.gen() < right.gen();
		});
		CHECK(capture.hits == copied);
	}
}

TEST_CASE("Observer - Is pair semantic inheritance") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all(ecs::Pair(ecs::Is, animal))
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	auto& observerIsData = wld.observers().data(observerIs);
	auto& observerQueryInfo = observerIsData.query.fetch();
	const auto& wolfEc = wld.fetch(wolf);
	CHECK(observerIsData.query.matches_any(observerQueryInfo, *wolfEc.pArchetype, ecs::EntitySpan{&wolf, 1}));
	CHECK(hits == 2);
	CHECK(observed.size() == 2);
	if (observed.size() >= 2)
		CHECK(observed[1] == wolf);
}

TEST_CASE("Observer - direct Is pair with direct stored edges") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	ecs::QueryTermOptions directOpts;
	directOpts.direct();

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all(ecs::Pair(ecs::Is, animal), directOpts)
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
}

TEST_CASE("Observer - direct Is pair via QueryInput with direct stored edges") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	ecs::QueryInput item{};
	item.op = ecs::QueryOpKind::All;
	item.access = ecs::QueryAccess::None;
	item.id = ecs::Pair(ecs::Is, animal);
	item.matchKind = ecs::QueryMatchKind::Direct;

	int hits = 0;
	cnt::darr<ecs::Entity> observed;
	const auto observerIs = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.add(item)
															.on_each([&](ecs::Iter& it) {
																auto entities = it.view<ecs::Entity>();
																GAIA_EACH(it) {
																	++hits;
																	observed.push_back(entities[i]);
																}
															})
															.entity();
	(void)observerIs;

	wld.as(mammal, animal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
	if (!observed.empty())
		CHECK(observed[0] == mammal);

	wld.as(wolf, mammal);
	CHECK(hits == 1);
	CHECK(observed.size() == 1);
}

TEST_CASE("Observer - is sugar matches semantic and direct Is terms") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int semanticHits = 0;
	const auto obsSemantic = wld.observer()
															 .event(ecs::ObserverEvent::OnAdd)
															 .is(animal)
															 .on_each([&](ecs::Iter&) {
																 ++semanticHits;
															 })
															 .entity();

	int directHits = 0;
	const auto obsDirect = wld.observer()
														 .event(ecs::ObserverEvent::OnAdd)
														 .is(animal, ecs::QueryTermOptions{}.direct())
														 .on_each([&](ecs::Iter&) {
															 ++directHits;
														 })
														 .entity();

	wld.as(mammal, animal);
	CHECK(semanticHits == 1);
	CHECK(directHits == 1);

	wld.as(wolf, mammal);
	CHECK(semanticHits == 2);
	CHECK(directHits == 1);

	(void)obsSemantic;
	(void)obsDirect;
}

TEST_CASE("Observer - in sugar matches descendants but excludes the base entity") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto mammal = wld.add();
	const auto wolf = wld.add();

	int hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.in(animal)
														.on_each([&](ecs::Iter&) {
															++hits;
														})
														.entity();

	wld.as(mammal, animal);
	CHECK(hits == 1);

	wld.as(wolf, mammal);
	CHECK(hits == 2);

	(void)observer;
}

TEST_CASE("Observer - prefabs are excluded by default and can be matched explicitly") {
	TestWorld twld;

	uint32_t defaultHits = 0;
	const auto obsDefault = wld.observer()
															.event(ecs::ObserverEvent::OnAdd)
															.all<Position>()
															.on_each([&](ecs::Iter&) {
																++defaultHits;
															})
															.entity();

	uint32_t prefabHits = 0;
	const auto obsMatchPrefab = wld.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.all<Position>()
																	.match_prefab()
																	.on_each([&](ecs::Iter&) {
																		++prefabHits;
																	})
																	.entity();

	const auto prefab = wld.prefab();
	const auto entity = wld.add();

	wld.add<Position>(prefab, {1, 0, 0});
	wld.add<Position>(entity, {2, 0, 0});

	CHECK(defaultHits == 1);
	CHECK(prefabHits == 2);

	(void)obsDefault;
	(void)obsMatchPrefab;
}

TEST_CASE("Observer - inherited prefab data matches on instantiate") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	const auto instance = wld.instantiate(prefab);

	CHECK(hits == 1);
	CHECK(wld.has(instance, position));
	CHECK_FALSE(wld.has_direct(instance, position));

	(void)observer;
}

TEST_CASE("Observer - inherited Is data matches when derived entity is linked to a base") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	wld.as(rabbit, animal);

	CHECK(hits == 1);
	CHECK(wld.has(rabbit, position));
	CHECK_FALSE(wld.has_direct(rabbit, position));

	(void)observer;
}

TEST_CASE("Observer - inherited Is override and local ownership") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	CHECK(wld.override<Position>(rabbit));
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited prefab data matches on instantiate_n") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	cnt::darray<ecs::Entity> instances;
	wld.instantiate_n(prefab, 5, [&](ecs::Entity instance) {
		instances.push_back(instance);
	});

	CHECK(hits == 5);
	CHECK(instances.size() == 5);
	for (const auto instance: instances) {
		CHECK(wld.has(instance, position));
		CHECK_FALSE(wld.has_direct(instance, position));
	}

	(void)observer;
}

TEST_CASE("Observer - prefab sync copied data matches existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	Position pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view<Position>();
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	auto prefabBuilder = wld.build(prefab);
	prefabBuilder.add<Position>();
	prefabBuilder.commit();
	wld.set<Position>(prefab) = {7.0f, 8.0f, 9.0f};

	hits = 0;
	observed = ecs::EntityBad;
	pos = {};

	CHECK(wld.sync(prefab) == 1);
	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(7.0f));
	CHECK(pos.y == doctest::Approx(8.0f));
	CHECK(pos.z == doctest::Approx(9.0f));

	(void)observer;
}

TEST_CASE("Observer - prefab sync sparse copied data matches existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view_any<PositionSparse>();
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.add<PositionSparse>(prefab, {2.0f, 3.0f, 4.0f});

	hits = 0;
	observed = ecs::EntityBad;
	pos = {};

	CHECK(wld.sync(prefab) == 1);
	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab data matches on delete from prefab") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	Position pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view_any<Position>(0);
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));
	CHECK_FALSE(wld.has<Position>(instance));
	CHECK(wld.sync(prefab) == 0);
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited sparse prefab data matches on delete from prefab") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto positionSparse = wld.add<PositionSparse>().entity;
	wld.add(positionSparse, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<PositionSparse>(prefab, {2.0f, 3.0f, 4.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view_any<PositionSparse>(0);
															observed = entityView[0];
															pos = posView[0];
														})
														.entity();

	wld.del<PositionSparse>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));
	CHECK_FALSE(wld.has<PositionSparse>(instance));
	CHECK(wld.sync(prefab) == 0);
	CHECK(hits == 1);

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal does not fire when another source still provides the term") {
	TestWorld twld;

	const auto root = wld.prefab();
	const auto branchA = wld.prefab();
	const auto branchB = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	wld.as(branchA, root);
	wld.as(branchB, root);
	wld.add<Position>(branchA, {1.0f, 2.0f, 3.0f});
	wld.add<Position>(branchB, {4.0f, 5.0f, 6.0f});

	const auto instance = wld.instantiate(branchA);
	wld.as(instance, branchB);

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter&) {
															++hits;
														})
														.entity();

	wld.del<Position>(branchA);

	CHECK(hits == 0);
	CHECK(wld.has<Position>(instance));
	const auto& pos = wld.get<Position>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal triggers no<T> OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {9.0f, 8.0f, 7.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.no<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observed == instance);
	CHECK_FALSE(wld.has<Position>(instance));

	(void)observer;
}

TEST_CASE("Observer - inherited prefab removal typed OnDel payload") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {11.0f, 12.0f, 13.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	Position observedPos{};
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<const Position>()
														.on_each([&](const Position& pos) {
															++hits;
															observedPos = pos;
														})
														.entity();

	wld.del<Position>(prefab);

	CHECK(hits == 1);
	CHECK(observedPos.x == doctest::Approx(11.0f));
	CHECK(observedPos.y == doctest::Approx(12.0f));
	CHECK(observedPos.z == doctest::Approx(13.0f));
	CHECK_FALSE(wld.has<Position>(instance));

	(void)observer;
}

TEST_CASE("Observer - prefab sync spawned child matches Parent pair") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto rootInstance = wld.instantiate(rootPrefab);
	const auto childPrefab = wld.prefab();

	uint32_t hits = 0;
	ecs::Entity observedChild = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all(ecs::Pair(ecs::Parent, rootInstance))
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observedChild = entityView[0];
														})
														.entity();

	auto childBuilder = wld.build(childPrefab);
	childBuilder.add(ecs::Pair(ecs::Parent, rootPrefab));
	childBuilder.commit();

	hits = 0;
	observedChild = ecs::EntityBad;

	CHECK(wld.sync(rootPrefab) == 1);
	CHECK(hits == 1);
	CHECK(observedChild != ecs::EntityBad);
	CHECK(wld.has_direct(observedChild, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has_direct(observedChild, ecs::Pair(ecs::Is, childPrefab)));

	(void)observer;
}

TEST_CASE("Observer - prefab sync child removal emits no OnDel") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	wld.parent(childPrefab, rootPrefab);
	const auto rootInstance = wld.instantiate(rootPrefab);

	ecs::Entity childInstance = ecs::EntityBad;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = child;
	});

	CHECK(childInstance != ecs::EntityBad);
	if (childInstance == ecs::EntityBad)
		return;

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(ecs::Pair(ecs::Parent, rootInstance))
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del(childPrefab, ecs::Pair(ecs::Parent, rootPrefab));
	CHECK(wld.sync(rootPrefab) == 0);

	CHECK(hits == 0);
	CHECK(observed == ecs::EntityBad);
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Parent, rootInstance)));

	(void)observer;
}

TEST_CASE("Observer - prefab sync removed copied override emits no OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del<Position>(prefab);
	CHECK(wld.sync(prefab) == 0);

	CHECK(hits == 0);
	CHECK(observed == ecs::EntityBad);
	CHECK(wld.has<Position>(instance));
	const auto& pos = wld.get<Position>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));

	(void)observer;
}

TEST_CASE("Observer - prefab sync removed sparse copied override emits no OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4.0f, 5.0f, 6.0f});
	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	wld.del<PositionSparse>(prefab);
	CHECK(wld.sync(prefab) == 0);

	CHECK(hits == 0);
	CHECK(observed == ecs::EntityBad);
	CHECK(wld.has<PositionSparse>(instance));
	const auto& pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(4.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(6.0f));

	(void)observer;
}

TEST_CASE("Observer - del runtime sparse id payload") {
	TestWorld twld;

	const auto sparseId = wld.add<PositionSparse>().entity;

	uint32_t hits = 0;
	ecs::Entity observedEntity = ecs::EntityBad;
	PositionSparse pos{};

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(sparseId)
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															auto posView = it.view_any<PositionSparse>(0);
															observedEntity = entityView[0];
															pos = posView[0];
														})
														.entity();

	const auto e = wld.add();
	wld.add<PositionSparse>(e, {21.0f, 22.0f, 23.0f});

	hits = 0;
	observedEntity = ecs::EntityBad;
	pos = {};

	wld.del(e, sparseId);

	CHECK(hits == 1);
	CHECK(observedEntity == e);
	CHECK(pos.x == doctest::Approx(21.0f));
	CHECK(pos.y == doctest::Approx(22.0f));
	CHECK(pos.z == doctest::Approx(23.0f));

	(void)observer;
}

TEST_CASE("Observer - prefab sparse override data matches on instantiate") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4, 0, 0});

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	const auto instance = wld.instantiate(prefab);

	CHECK(hits == 1);
	CHECK(wld.has<PositionSparse>(instance));
	(void)observer;
}

TEST_CASE("Observer - prefab sparse override data matches on instantiate_n") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<PositionSparse>(prefab, {4, 0, 0});

	uint32_t hits = 0;
	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnAdd)
														.all<PositionSparse>()
														.on_each([&](ecs::Iter& it) {
															hits += it.size();
														})
														.entity();

	cnt::darray<ecs::Entity> instances;
	wld.instantiate_n(prefab, 5, [&](ecs::Entity instance) {
		instances.push_back(instance);
	});

	CHECK(hits == 5);
	CHECK(instances.size() == 5);
	for (const auto instance: instances)
		CHECK(wld.has<PositionSparse>(instance));

	(void)observer;
}

TEST_CASE("Observer - add(QueryInput) registration and fast path") {
	TestWorld twld;

	const auto positionTerm = wld.add<Position>().entity;

	ecs::QueryInput simpleItem{};
	simpleItem.op = ecs::QueryOpKind::All;
	simpleItem.access = ecs::QueryAccess::Read;
	simpleItem.id = positionTerm;

	uint32_t hits = 0;
	const auto observerSimple = wld.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.add(simpleItem)
																	.on_each([&hits](ecs::Iter&) {
																		++hits;
																	})
																	.entity();

	const auto& dataSimple = wld.observers().data(observerSimple);
	CHECK(dataSimple.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	CHECK(hits == 1);

	ecs::QueryInput srcItem{};
	srcItem.op = ecs::QueryOpKind::All;
	srcItem.access = ecs::QueryAccess::Read;
	srcItem.id = positionTerm;
	srcItem.entSrc = wld.add();

	const auto observerSrc =
			wld.observer().event(ecs::ObserverEvent::OnAdd).add(srcItem).on_each([](ecs::Iter&) {}).entity();

	const auto& dataSrc = wld.observers().data(observerSrc);
	CHECK(dataSrc.plan.fastPath == ecs::ObserverPlan::FastPath::Disabled);
}

TEST_CASE("Observer - single negative fast path runtime") {
	TestWorld twld;

	uint32_t onAddHits = 0;
	uint32_t onDelHits = 0;

	const auto observerOnAddNoPos = wld.observer()
																			.event(ecs::ObserverEvent::OnAdd)
																			.no<Position>()
																			.on_each([&onAddHits](ecs::Iter&) {
																				++onAddHits;
																			})
																			.entity();
	const auto observerOnDelNoPos = wld.observer()
																			.event(ecs::ObserverEvent::OnDel)
																			.no<Position>()
																			.on_each([&onDelHits](ecs::Iter&) {
																				++onDelHits;
																			})
																			.entity();

	const auto& dataOnAddNoPos = wld.observers().data(observerOnAddNoPos);
	const auto& dataOnDelNoPos = wld.observers().data(observerOnDelNoPos);
	CHECK(dataOnAddNoPos.plan.fastPath == ecs::ObserverPlan::FastPath::SingleNegativeTerm);
	CHECK(dataOnDelNoPos.plan.fastPath == ecs::ObserverPlan::FastPath::SingleNegativeTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	CHECK(onAddHits == 0);
	CHECK(onDelHits == 0);

	wld.del<Position>(e);
	CHECK(onAddHits == 0);
	CHECK(onDelHits == 1);
}

TEST_CASE("Observer - single positive OnDel fast path runtime") {
	TestWorld twld;

	uint32_t onDelHits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observerOnDelPos = wld.observer()
																		.event(ecs::ObserverEvent::OnDel)
																		.all<Position>()
																		.on_each([&](ecs::Iter& it) {
																			++onDelHits;
																			auto entityView = it.view<ecs::Entity>();
																			observed = entityView[0];
																		})
																		.entity();

	const auto& dataOnDelPos = wld.observers().data(observerOnDelPos);
	CHECK(dataOnDelPos.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add<Position>(e, {});
	onDelHits = 0;
	observed = ecs::EntityBad;

	wld.del<Position>(e);
	CHECK(onDelHits == 1);
	CHECK(observed == e);
}

TEST_CASE("Observer - fixed pair OnDel fast path runtime") {
	TestWorld twld;

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto pair = ecs::Pair(relation, target);

	uint32_t hits = 0;
	ecs::Entity observed = ecs::EntityBad;

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all(pair)
														.on_each([&](ecs::Iter& it) {
															++hits;
															auto entityView = it.view<ecs::Entity>();
															observed = entityView[0];
														})
														.entity();

	const auto& data = wld.observers().data(observer);
	CHECK(data.plan.fastPath == ecs::ObserverPlan::FastPath::SinglePositiveTerm);

	const auto e = wld.add();
	wld.add(e, pair);
	hits = 0;
	observed = ecs::EntityBad;

	wld.del(e, pair);
	CHECK(hits == 1);
	CHECK(observed == e);

	(void)observer;
}

TEST_CASE("Observer - deleting prefab entity leaves instances and emits no standalone inherited OnDel") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {31.0f, 32.0f, 33.0f});

	const auto a = wld.instantiate(prefab);
	const auto b = wld.instantiate(prefab);

	uint32_t hits = 0;
	cnt::darray<ecs::Entity> observed;
	observed.reserve(2);

	const auto observer = wld.observer()
														.event(ecs::ObserverEvent::OnDel)
														.all<Position>()
														.on_each([&](ecs::Iter& it) {
															auto entityView = it.view<ecs::Entity>();
															GAIA_EACH(it) {
																++hits;
																observed.push_back(entityView[i]);
															}
														})
														.entity();

	wld.del(prefab);
	wld.update();

	CHECK(hits == 0);
	CHECK(observed.empty());
	CHECK_FALSE(wld.valid(prefab));
	CHECK(wld.valid(a));
	CHECK(wld.valid(b));

	(void)observer;
}

#endif
