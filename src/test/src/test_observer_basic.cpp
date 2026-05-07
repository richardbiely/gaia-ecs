#include "test_common.h"

#define TestWorld SparseTestWorld
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

TEST_CASE("Observer - invalid kind reports reason") {
	TestWorld twld;

	const auto observerEntity = wld.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.kind(ecs::QueryCacheKind::All)
																	.no<Position>()
																	.on_each([](ecs::Iter&) {})
																	.entity();

	auto& obsData = wld.observers().data(observerEntity);
	CHECK_FALSE(obsData.query.valid());
	CHECK(obsData.query.kind_error() == ecs::QueryKindRes::AllNotIm);
	CHECK(std::string(obsData.query.kind_error_str()).find("immediate") != std::string::npos);
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

	const auto& runtimeTableComp = add_runtime_component(
			wld, "Observer_Runtime_Table_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	const auto& runtimeSparseComp = add_runtime_component(
			wld, "Observer_Runtime_Sparse_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
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

	const auto& runtimeTableComp = add_runtime_component(
			wld, "Observer_Runtime_Table_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Table,
			(uint32_t)alignof(Position));
	const auto& runtimeSparseComp = add_runtime_component(
			wld, "Observer_Runtime_Sparse_Position_Setter", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
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
		const auto pos = wld.get<Position>(e);
		CHECK(pos.x == doctest::Approx(40.0f));
		CHECK(pos.y == doctest::Approx(41.0f));
		CHECK(pos.z == doctest::Approx(42.0f));
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
		const auto pos = wld.get<Position>(e);
		CHECK(pos.x == doctest::Approx(50.0f));
		CHECK(pos.y == doctest::Approx(51.0f));
		CHECK(pos.z == doctest::Approx(52.0f));
	}

	SUBCASE("Iter any callback") {
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
																		 auto posView = it.view_any_mut<Position>(0);
																		 CHECK(onSetHits == 0);
																		 GAIA_EACH(it) {
																			 posView[i].x = 60.0f;
																			 posView[i].y = 61.0f;
																			 posView[i].z = 62.0f;
																			 CHECK(onSetHits == 0);
																		 }
																	 })
																	 .entity();
		(void)onAddObserver;

		const auto e = wld.add();
		wld.add<Position>(e, {7.0f, 8.0f, 9.0f});

		CHECK(onSetHits == 1);
		CHECK(lastPos.x == doctest::Approx(60.0f));
		CHECK(lastPos.y == doctest::Approx(61.0f));
		CHECK(lastPos.z == doctest::Approx(62.0f));
		const auto pos = wld.get<Position>(e);
		CHECK(pos.x == doctest::Approx(60.0f));
		CHECK(pos.y == doctest::Approx(61.0f));
		CHECK(pos.z == doctest::Approx(62.0f));
	}
}

TEST_CASE("Observer - SoA callbacks materialize direct payloads correctly") {
	SUBCASE("typed OnSet callback") {
		TestWorld twld;

		uint32_t hits = 0;
		PositionSoA lastPos{};
		const auto observer = wld.observer()
															.event(ecs::ObserverEvent::OnSet)
															.all<PositionSoA>()
															.on_each([&](const PositionSoA& pos) {
																++hits;
																lastPos = pos;
															})
															.entity();
		(void)observer;

		const auto e = wld.add();
		wld.add<PositionSoA>(e, {1.0f, 2.0f, 3.0f});
		hits = 0;

		wld.set<PositionSoA>(e) = PositionSoA{4.0f, 5.0f, 6.0f};

		CHECK(hits == 1);
		CHECK(lastPos.x == doctest::Approx(4.0f));
		CHECK(lastPos.y == doctest::Approx(5.0f));
		CHECK(lastPos.z == doctest::Approx(6.0f));
	}

	SUBCASE("Iter callback can read direct SoA payload") {
		TestWorld twld;

		uint32_t hits = 0;
		float lastX = 0.0f;
		float lastY = 0.0f;
		float lastZ = 0.0f;
		const auto observer = wld.observer()
															.event(ecs::ObserverEvent::OnSet)
															.all<PositionSoA>()
															.on_each([&](ecs::Iter& it) {
																++hits;
																auto posView = it.view_any<PositionSoA>(0);
																auto xs = posView.template get<0>();
																auto ys = posView.template get<1>();
																auto zs = posView.template get<2>();
																CHECK(it.size() == 1);
																lastX = xs[0];
																lastY = ys[0];
																lastZ = zs[0];
															})
															.entity();
		(void)observer;

		const auto e = wld.add();
		wld.add<PositionSoA>(e, {7.0f, 8.0f, 9.0f});
		hits = 0;

		wld.set<PositionSoA>(e) = PositionSoA{10.0f, 11.0f, 12.0f};

		CHECK(hits == 1);
		CHECK(lastX == doctest::Approx(10.0f));
		CHECK(lastY == doctest::Approx(11.0f));
		CHECK(lastZ == doctest::Approx(12.0f));
	}

	SUBCASE("Iter any write callback emits SoA OnSet after callback completion") {
		TestWorld twld;

		uint32_t onSetHits = 0;
		PositionSoA lastPos{};
		const auto onSetObserver = wld.observer()
																	 .event(ecs::ObserverEvent::OnSet)
																	 .all<PositionSoA>()
																	 .on_each([&](const PositionSoA& pos) {
																		 ++onSetHits;
																		 lastPos = pos;
																	 })
																	 .entity();
		(void)onSetObserver;

		const auto onAddObserver = wld.observer()
																	 .event(ecs::ObserverEvent::OnAdd)
																	 .all<PositionSoA&>()
																	 .on_each([&](ecs::Iter& it) {
																		 auto posView = it.view_any_mut<PositionSoA>(0);
																		 auto xs = posView.template set<0>();
																		 auto ys = posView.template set<1>();
																		 auto zs = posView.template set<2>();
																		 CHECK(onSetHits == 0);
																		 CHECK(it.size() == 1);
																		 xs[0] = 13.0f;
																		 ys[0] = 14.0f;
																		 zs[0] = 15.0f;
																		 CHECK(onSetHits == 0);
																	 })
																	 .entity();
		(void)onAddObserver;

		const auto e = wld.add();
		wld.add<PositionSoA>(e, {1.0f, 2.0f, 3.0f});

		CHECK(onSetHits == 1);
		CHECK(lastPos.x == doctest::Approx(13.0f));
		CHECK(lastPos.y == doctest::Approx(14.0f));
		CHECK(lastPos.z == doctest::Approx(15.0f));
		const auto pos = wld.get<PositionSoA>(e);
		CHECK(pos.x == doctest::Approx(13.0f));
		CHECK(pos.y == doctest::Approx(14.0f));
		CHECK(pos.z == doctest::Approx(15.0f));
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

TEST_CASE("Observer - relation wildcard observes add and delete") {
	TestWorld twld;

	const auto homeOf = wld.add();
	const auto homeA = wld.add();
	const auto homeB = wld.add();
	const auto citizen = wld.add();

	uint32_t addHits = 0;
	uint32_t delHits = 0;

	const auto onAdd = wld.observer()
												 .event(ecs::ObserverEvent::OnAdd)
												 .all(ecs::Pair(homeOf, ecs::All))
												 .on_each([&](ecs::Iter&) {
													 ++addHits;
												 })
												 .entity();
	const auto onDel = wld.observer()
												 .event(ecs::ObserverEvent::OnDel)
												 .all(ecs::Pair(homeOf, ecs::All))
												 .on_each([&](ecs::Iter&) {
													 ++delHits;
												 })
												 .entity();
	(void)onAdd;
	(void)onDel;

	wld.add(citizen, ecs::Pair(homeOf, homeA));
	CHECK(addHits == 1);
	CHECK(delHits == 0);

	wld.del(citizen, ecs::Pair(homeOf, homeA));
	CHECK(addHits == 1);
	CHECK(delHits == 1);

	wld.add(citizen, ecs::Pair(homeOf, homeB));
	CHECK(addHits == 2);
	CHECK(delHits == 1);
}

TEST_CASE("Observer - exact relation pair observes only its target") {
	TestWorld twld;

	const auto homeOf = wld.add();
	const auto homeA = wld.add();
	const auto homeB = wld.add();
	const auto citizenA = wld.add();
	const auto citizenB = wld.add();

	uint32_t hits = 0;
	const auto obs = wld.observer()
											 .event(ecs::ObserverEvent::OnAdd)
											 .all(ecs::Pair(homeOf, homeA))
											 .on_each([&](ecs::Iter&) {
												 ++hits;
											 })
											 .entity();
	(void)obs;

	wld.add(citizenA, ecs::Pair(homeOf, homeB));
	CHECK(hits == 0);

	wld.add(citizenB, ecs::Pair(homeOf, homeA));
	CHECK(hits == 1);
}

TEST_CASE("Observer - relation retarget is delete and add") {
	TestWorld twld;

	const auto homeOf = wld.add();
	const auto homeA = wld.add();
	const auto homeB = wld.add();
	const auto citizen = wld.add();

	uint32_t addHits = 0;
	uint32_t delHits = 0;
	uint32_t setHits = 0;

	const auto onAdd = wld.observer()
												 .event(ecs::ObserverEvent::OnAdd)
												 .all(ecs::Pair(homeOf, ecs::All))
												 .on_each([&](ecs::Iter&) {
													 ++addHits;
												 })
												 .entity();
	const auto onDel = wld.observer()
												 .event(ecs::ObserverEvent::OnDel)
												 .all(ecs::Pair(homeOf, ecs::All))
												 .on_each([&](ecs::Iter&) {
													 ++delHits;
												 })
												 .entity();
	const auto onSet = wld.observer()
												 .event(ecs::ObserverEvent::OnSet)
												 .all(ecs::Pair(homeOf, ecs::All))
												 .on_each([&](ecs::Iter&) {
													 ++setHits;
												 })
												 .entity();
	(void)onAdd;
	(void)onDel;
	(void)onSet;

	wld.add(citizen, ecs::Pair(homeOf, homeA));
	CHECK(addHits == 1);
	CHECK(delHits == 0);
	CHECK(setHits == 0);

	wld.del(citizen, ecs::Pair(homeOf, homeA));
	wld.add(citizen, ecs::Pair(homeOf, homeB));

	CHECK(addHits == 2);
	CHECK(delHits == 1);
	CHECK(setHits == 0);
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

#endif
