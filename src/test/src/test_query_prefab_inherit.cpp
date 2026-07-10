#include "test_common.h"

#define TestWorld SparseTestWorld

TEST_CASE("Prefab - instantiate creates a non-prefab instance with copied data") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto prefabAnimal = wld.prefab();
	wld.as(prefabAnimal, animal);
	wld.add<Position>(prefabAnimal, {7, 0, 0});

	int positionHits = 0;
	wld.observer().event(ecs::ObserverEvent::OnAdd).all<Position>().on_each([&](ecs::Iter&) {
		++positionHits;
	});

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
	CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefabAnimal)));
	CHECK_FALSE(wld.has_direct(instance, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.has(instance, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.get<Position>(instance).x == 7.0f);
	CHECK(positionHits == 1);

	auto qDefault = wld.query().is(prefabAnimal);
	CHECK(qDefault.count() == 1);
	expect_exact_entities(qDefault, {instance});

	auto qPrefab = wld.query().is(prefabAnimal).match_prefab();
	CHECK(qPrefab.count() == 2);
	expect_exact_entities(qPrefab, {prefabAnimal, instance});
}

TEST_CASE("Prefab - instantiate does not copy the prefab name") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	wld.name(prefabAnimal, "prefab_animal");

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK(!wld.name(prefabAnimal).empty());
	CHECK(wld.name(prefabAnimal) == "prefab_animal");
	CHECK(wld.name(instance).empty());
}

TEST_CASE("Prefab - instantiate respects DontInherit policy") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {7, 0, 0});
	wld.add<Scale>(prefabAnimal, {3, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::DontInherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has<Position>(instance));
	CHECK(wld.has<Scale>(instance));
	CHECK(wld.get<Scale>(instance).x == 3.0f);
	CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefabAnimal)));
}

TEST_CASE("Prefab - explicit Override policy still copies data") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {9, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Override));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK(wld.has<Position>(instance));
	CHECK(wld.get<Position>(instance).x == 9.0f);
}

TEST_CASE("Prefab - explicit Override policy copies sparse data") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSparse>().entity;
	wld.add<PositionSparse>(prefabAnimal, {9, 1, 2});

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK(wld.has_direct(instance, position));
	const auto pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(9.0f));
	CHECK(pos.y == doctest::Approx(1.0f));
	CHECK(pos.z == doctest::Approx(2.0f));

	const auto prefabPos = wld.get<PositionSparse>(prefabAnimal);
	CHECK(prefabPos.x == doctest::Approx(9.0f));
	CHECK(prefabPos.y == doctest::Approx(1.0f));
	CHECK(prefabPos.z == doctest::Approx(2.0f));
}

TEST_CASE("Prefab - Inherit policy before local override") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.has<Position>(instance));
	CHECK(wld.get<Position>(instance).x == 5.0f);

	wld.add<Position>(instance, {8, 0, 0});
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == 8.0f);
}

TEST_CASE("Prefab - inherited query survives empty before iteration") {
	TestWorld twld;

	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {1, 2, 3});

	static constexpr uint32_t InstanceCnt = 128;
	wld.instantiate_n(prefab, InstanceCnt);

	auto q = wld.query().all<Position>().is(prefab);
	CHECK_FALSE(q.empty());

	uint32_t cntEach = 0;
	uint64_t sumEach = 0;
	q.each([&](const Position& p) {
		sumEach += (uint64_t)(p.x + p.y + p.z);
		++cntEach;
	});
	CHECK(cntEach == InstanceCnt);
	CHECK(sumEach == 6ULL * InstanceCnt);
}

TEST_CASE("Prefab - explicit override and inherited ownership") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.override<Position>(instance));
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(5.0f));
	CHECK_FALSE(wld.override<Position>(instance));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - explicit override and inherited tags") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto tag = wld.add();
	wld.add(prefabAnimal, tag);
	wld.add(tag, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, tag));
	CHECK(wld.has(instance, tag));
	CHECK(wld.override(instance, tag));
	CHECK(wld.has_direct(instance, tag));
	CHECK_FALSE(wld.override(instance, tag));
}

TEST_CASE("Is inheritance - explicit override and inherited ownership") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	CHECK_FALSE(wld.has_direct(rabbit, position));
	CHECK(wld.has<Position>(rabbit));
	CHECK(wld.get<Position>(rabbit).x == doctest::Approx(5.0f));
	CHECK(wld.override<Position>(rabbit));
	CHECK(wld.has_direct(rabbit, position));
	CHECK(wld.get<Position>(rabbit).x == doctest::Approx(5.0f));
	CHECK_FALSE(wld.override<Position>(rabbit));
	CHECK(wld.get<Position>(animal).x == doctest::Approx(5.0f));
}

TEST_CASE("Is inheritance - explicit override and inherited tags") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto tag = wld.add();
	wld.add(animal, tag);
	wld.add(tag, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	CHECK_FALSE(wld.has_direct(rabbit, tag));
	CHECK(wld.has(rabbit, tag));
	CHECK(wld.override(rabbit, tag));
	CHECK(wld.has_direct(rabbit, tag));
	CHECK_FALSE(wld.override(rabbit, tag));
}

TEST_CASE("Prefab - inherited component query writes local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<Position>();
	CHECK(qRead.count() == 1);
	expect_exact_entities(qRead, {instance});

	float xRead = 0.0f;
	qRead.each([&](const Position& pos) {
		xRead += pos.x;
	});
	CHECK(xRead == doctest::Approx(5.0f));

	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](Position& pos) {
		pos.x += 3.0f;
	});

	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(8.0f));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Is inheritance - inherited component queries see derived entities and create local overrides on write") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	auto qRead = wld.query().all<Position>();
	CHECK(qRead.count() == 2);
	expect_exact_entities(qRead, {animal, rabbit});

	float xRead = 0.0f;
	qRead.each([&](const Position& pos) {
		xRead += pos.x;
	});
	CHECK(xRead == doctest::Approx(10.0f));

	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](Position& pos) {
		pos.x += 3.0f;
	});

	CHECK(wld.has_direct(rabbit, position));
	CHECK(wld.get<Position>(animal).x == doctest::Approx(8.0f));
	CHECK(wld.get<Position>(rabbit).x == doctest::Approx(11.0f));
}

TEST_CASE("Is inheritance - explicit override and query membership") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	auto q = wld.query().all<Position>();
	CHECK(q.count() == 2);
	expect_exact_entities(q, {animal, rabbit});

	CHECK(wld.override<Position>(rabbit));
	CHECK(q.count() == 2);
	expect_exact_entities(q, {animal, rabbit});
}

TEST_CASE("Prefab - inherited sparse component query writes local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSparse>().entity;
	wld.add<PositionSparse>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<PositionSparse>();
	CHECK(qRead.count() == 1);
	expect_exact_entities(qRead, {instance});

	float xRead = 0.0f;
	qRead.each([&](const PositionSparse& pos) {
		xRead += pos.x;
	});
	CHECK(xRead == doctest::Approx(5.0f));

	auto qWrite = wld.query().all<PositionSparse&>();
	qWrite.each([&](PositionSparse& pos) {
		pos.x += 3.0f;
	});

	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<PositionSparse>(instance).x == doctest::Approx(8.0f));
	CHECK(wld.get<PositionSparse>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - inherited writable query over a stable snapshot") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instanceA = wld.instantiate(prefabAnimal);
	const auto instanceB = wld.instantiate(prefabAnimal);

	uint32_t hits = 0;
	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](Position& pos) {
		++hits;
		pos.x += 2.0f;
	});

	CHECK(hits == 2);
	CHECK(wld.has_direct(instanceA, position));
	CHECK(wld.has_direct(instanceB, position));
	CHECK(wld.get<Position>(instanceA).x == doctest::Approx(7.0f));
	CHECK(wld.get<Position>(instanceB).x == doctest::Approx(7.0f));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - inherited Iter query writes local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instanceA = wld.instantiate(prefabAnimal);
	const auto instanceB = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<Position>();
	float xRead = 0.0f;
	uint32_t rowsRead = 0;
	qRead.each([&](ecs::Iter& it) {
		auto posView = it.view_any<Position>(1);
		GAIA_EACH(it) {
			xRead += posView[i].x;
			++rowsRead;
		}
	});
	CHECK(rowsRead == 2);
	CHECK(xRead == doctest::Approx(10.0f));

	auto qWrite = wld.query().all<Position&>();
	qWrite.each([&](ecs::Iter& it) {
		auto posView = it.view_any_mut<Position>(1);
		GAIA_EACH(it) {
			posView[i].x += 4.0f;
		}
	});

	CHECK(wld.has_direct(instanceA, position));
	CHECK(wld.has_direct(instanceB, position));
	CHECK(wld.get<Position>(instanceA).x == doctest::Approx(9.0f));
	CHECK(wld.get<Position>(instanceB).x == doctest::Approx(9.0f));
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - inherited Iter query swaps inherited payload across archetypes") {
	TestWorld twld;

	const auto prefabA = wld.prefab();
	const auto prefabB = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabA, {5, 1, 0});
	wld.add<Position>(prefabB, {9, 2, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instanceA = wld.instantiate(prefabA);
	const auto instanceB = wld.instantiate(prefabB);

	uint32_t batchCnt = 0;
	uint32_t rowCnt = 0;
	float sumX = 0.0f;
	float sumY = 0.0f;
	cnt::darray<int32_t> firstXs;
	auto q = wld.query().all<Position>();
	q.each([&](ecs::Iter& it) {
		auto posView = it.view_any<Position>(1);
		CHECK(posView.data() == nullptr);
		CHECK(it.size() == 1);
		++batchCnt;
		firstXs.push_back((int32_t)posView[0].x);
		GAIA_EACH(it) {
			++rowCnt;
			sumX += posView[i].x;
			sumY += posView[i].y;
		}
	});

	CHECK(batchCnt == 2);
	CHECK(rowCnt == 2);
	CHECK(sumX == doctest::Approx(14.0f));
	CHECK(sumY == doctest::Approx(3.0f));
	CHECK(core::has(firstXs, (int32_t)5));
	CHECK(core::has(firstXs, (int32_t)9));
	CHECK(wld.has(instanceA));
	CHECK(wld.has(instanceB));
}

TEST_CASE("Prefab - inherited Iter SoA query writes local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSoA>().entity;
	wld.add<PositionSoA>(prefabAnimal, {5, 6, 7});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	auto qRead = wld.query().all<PositionSoA>();
	float xRead = 0.0f;
	float yRead = 0.0f;
	qRead.each([&](ecs::Iter& it) {
		auto posView = it.view_any<PositionSoA>(1);
		auto xs = posView.template get<0>();
		auto ys = posView.template get<1>();
		GAIA_EACH(it) {
			xRead += xs[i];
			yRead += ys[i];
		}
	});
	CHECK(xRead == doctest::Approx(5.0f));
	CHECK(yRead == doctest::Approx(6.0f));

	auto qWrite = wld.query().all<PositionSoA&>();
	qWrite.each([&](ecs::Iter& it) {
		auto posView = it.view_any_mut<PositionSoA>(1);
		auto xs = posView.template set<0>();
		auto ys = posView.template set<1>();
		GAIA_EACH(it) {
			xs[i] = xs[i] + 3.0f;
			ys[i] = ys[i] + 4.0f;
		}
	});

	CHECK(wld.has_direct(instance, position));
	const auto pos = wld.get<PositionSoA>(instance);
	CHECK(pos.x == doctest::Approx(8.0f));
	CHECK(pos.y == doctest::Approx(10.0f));
	CHECK(pos.z == doctest::Approx(7.0f));

	const auto prefabPos = wld.get<PositionSoA>(prefabAnimal);
	CHECK(prefabPos.x == doctest::Approx(5.0f));
	CHECK(prefabPos.y == doctest::Approx(6.0f));
	CHECK(prefabPos.z == doctest::Approx(7.0f));
}

TEST_CASE("Prefab - explicit override and inherited SoA components") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSoA>().entity;
	wld.add<PositionSoA>(prefabAnimal, {5, 6, 7});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.override<PositionSoA>(instance));
	CHECK(wld.has_direct(instance, position));

	const auto pos = wld.get<PositionSoA>(instance);
	CHECK(pos.x == doctest::Approx(5.0f));
	CHECK(pos.y == doctest::Approx(6.0f));
	CHECK(pos.z == doctest::Approx(7.0f));
	CHECK_FALSE(wld.override<PositionSoA>(instance));
}

TEST_CASE("Prefab - explicit override and inherited sparse components") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<PositionSparse>().entity;
	wld.add<PositionSparse>(prefabAnimal, {5, 6, 7});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.override<PositionSparse>(instance));
	CHECK(wld.has_direct(instance, position));

	const auto pos = wld.get<PositionSparse>(instance);
	CHECK(pos.x == doctest::Approx(5.0f));
	CHECK(pos.y == doctest::Approx(6.0f));
	CHECK(pos.z == doctest::Approx(7.0f));
	CHECK_FALSE(wld.override<PositionSparse>(instance));
}

TEST_CASE("Prefab - explicit override by id and inherited runtime sparse components") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Sparse_Prefab_Position", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);
	wld.add(prefabAnimal, runtimeComp.entity, Position{2, 3, 4});
	wld.add(runtimeComp.entity, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, runtimeComp.entity));
	CHECK(wld.has(instance, runtimeComp.entity));
	CHECK(wld.override(instance, runtimeComp.entity));
	CHECK(wld.has_direct(instance, runtimeComp.entity));
	CHECK_FALSE(wld.override(instance, runtimeComp.entity));

	const auto& pos = wld.get<Position>(instance, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(2.0f));
	CHECK(pos.y == doctest::Approx(3.0f));
	CHECK(pos.z == doctest::Approx(4.0f));
}

TEST_CASE("Prefab - inherited runtime sparse component set by id writes local override") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto& runtimeComp = add_runtime_component(
			wld, "Runtime_Sparse_Prefab_Position_Query", (uint32_t)sizeof(Position), ecs::DataStorageType::Sparse,
			(uint32_t)alignof(Position));
	wld.add(runtimeComp.entity, ecs::DontFragment);
	wld.add(prefabAnimal, runtimeComp.entity, Position{2, 3, 4});
	wld.add(runtimeComp.entity, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefabAnimal);

	CHECK_FALSE(wld.has_direct(instance, runtimeComp.entity));
	CHECK(wld.has(instance, runtimeComp.entity));

	{
		auto pos = wld.set<Position>(instance, runtimeComp.entity);
		pos = {7, 10, 4};
	}

	CHECK(wld.has_direct(instance, runtimeComp.entity));

	const auto& pos = wld.get<Position>(instance, runtimeComp.entity);
	CHECK(pos.x == doctest::Approx(7.0f));
	CHECK(pos.y == doctest::Approx(10.0f));
	CHECK(pos.z == doctest::Approx(4.0f));

	const auto& prefabPos = wld.get<Position>(prefabAnimal, runtimeComp.entity);
	CHECK(prefabPos.x == doctest::Approx(2.0f));
	CHECK(prefabPos.y == doctest::Approx(3.0f));
	CHECK(prefabPos.z == doctest::Approx(4.0f));
}

TEST_CASE("Prefab - instantiate recurses Parent-owned prefab children") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.parent(leafPrefab, childPrefab);

	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	const auto rootInstance = wld.instantiate(rootPrefab);

	auto q = wld.query().all<Position>().match_prefab();
	cnt::darray<ecs::Entity> entities;
	q.arr(entities);

	ecs::Entity childInstance = ecs::EntityBad;
	ecs::Entity leafInstance = ecs::EntityBad;
	for (const auto entity: entities) {
		if (entity == rootPrefab || entity == childPrefab || entity == leafPrefab || entity == rootInstance)
			continue;
		if (wld.has_direct(entity, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = entity;
		else if (wld.has_direct(entity, ecs::Pair(ecs::Is, leafPrefab)))
			leafInstance = entity;
	}

	CHECK(childInstance != ecs::EntityBad);
	CHECK(leafInstance != ecs::EntityBad);

	CHECK(wld.has_direct(rootInstance, ecs::Pair(ecs::Is, rootPrefab)));
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Is, childPrefab)));
	CHECK(wld.has_direct(leafInstance, ecs::Pair(ecs::Is, leafPrefab)));

	CHECK(wld.has(childInstance, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has(leafInstance, ecs::Pair(ecs::Parent, childInstance)));

	CHECK_FALSE(wld.has_direct(rootInstance, ecs::Prefab));
	CHECK_FALSE(wld.has_direct(childInstance, ecs::Prefab));
	CHECK_FALSE(wld.has_direct(leafInstance, ecs::Prefab));

	CHECK(wld.get<Position>(rootInstance).x == 1.0f);
	CHECK(wld.get<Position>(childInstance).x == 2.0f);
	CHECK(wld.get<Position>(leafInstance).x == 3.0f);
}

TEST_CASE("Prefab - instantiate recurses ChildOf-owned prefab children") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.child(childPrefab, rootPrefab);
	wld.child(leafPrefab, childPrefab);

	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	const auto rootInstance = wld.instantiate(rootPrefab);
	const auto childInstance = wld.find_prefab_instance(rootInstance, childPrefab);
	const auto leafInstance = wld.find_prefab_instance(rootInstance, leafPrefab);

	CHECK(childInstance != ecs::EntityBad);
	CHECK(leafInstance != ecs::EntityBad);
	if (childInstance != ecs::EntityBad && leafInstance != ecs::EntityBad) {
		CHECK(wld.has(childInstance, ecs::Pair(ecs::ChildOf, rootInstance)));
		CHECK(wld.has(leafInstance, ecs::Pair(ecs::ChildOf, childInstance)));
		CHECK_FALSE(wld.has(childInstance, ecs::Pair(ecs::ChildOf, rootPrefab)));
		CHECK_FALSE(wld.has(leafInstance, ecs::Pair(ecs::ChildOf, childPrefab)));
		CHECK(wld.get<Position>(childInstance).x == doctest::Approx(2.0f));
		CHECK(wld.get<Position>(leafInstance).x == doctest::Approx(3.0f));
	}

	cnt::darray<ecs::Entity> roots;
	wld.instantiate_n(rootPrefab, 2, [&](ecs::Entity instance) {
		roots.push_back(instance);
	});

	CHECK(roots.size() == 2);
	for (const auto instance: roots) {
		const auto child = wld.find_prefab_instance(instance, childPrefab);
		const auto leaf = wld.find_prefab_instance(instance, leafPrefab);
		CHECK(child != ecs::EntityBad);
		CHECK(leaf != ecs::EntityBad);
		if (child == ecs::EntityBad || leaf == ecs::EntityBad)
			continue;

		CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, instance)));
		CHECK(wld.has(leaf, ecs::Pair(ecs::ChildOf, child)));
		CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));
		CHECK(wld.get<Position>(leaf).x == doctest::Approx(3.0f));
	}
}

TEST_CASE("Prefab - instantiate can parent the spawned subtree under an existing entity") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.parent(leafPrefab, childPrefab);

	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	const auto rootInstance = wld.instantiate(rootPrefab, scene);

	auto q = wld.query().all<Position>().match_prefab();
	cnt::darray<ecs::Entity> entities;
	q.arr(entities);

	ecs::Entity childInstance = ecs::EntityBad;
	ecs::Entity leafInstance = ecs::EntityBad;
	for (const auto entity: entities) {
		if (entity == rootPrefab || entity == childPrefab || entity == leafPrefab || entity == rootInstance)
			continue;
		if (wld.has_direct(entity, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = entity;
		else if (wld.has_direct(entity, ecs::Pair(ecs::Is, leafPrefab)))
			leafInstance = entity;
	}

	CHECK(rootInstance != ecs::EntityBad);
	CHECK(childInstance != ecs::EntityBad);
	CHECK(leafInstance != ecs::EntityBad);

	CHECK(wld.has(rootInstance, ecs::Pair(ecs::Parent, scene)));
	CHECK(wld.has(childInstance, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has(leafInstance, ecs::Pair(ecs::Parent, childInstance)));
}

TEST_CASE("Instantiate - non-prefab parented fallback as copy plus parent") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto animal = wld.add();
	wld.name(animal, "animal");
	wld.add<Position>(animal, {4, 5, 6});

	const auto instance = wld.instantiate(animal, scene);

	CHECK(instance != animal);
	CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
	CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
	CHECK_FALSE(wld.has_direct(instance, ecs::Pair(ecs::Is, animal)));
	CHECK(wld.name(instance).empty());
	CHECK(wld.get<Position>(instance).x == doctest::Approx(4.0f));
	CHECK(wld.get<Position>(instance).y == doctest::Approx(5.0f));
	CHECK(wld.get<Position>(instance).z == doctest::Approx(6.0f));
}

TEST_CASE("Prefab - instantiate_n spawns multiple prefab instances") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	wld.add<Position>(prefabAnimal, {1, 2, 3});

	cnt::darray<ecs::Entity> roots;
	wld.instantiate_n(prefabAnimal, 4, [&](ecs::Entity instance) {
		roots.push_back(instance);
	});

	CHECK(roots.size() == 4);
	for (const auto instance: roots) {
		CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
		CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefabAnimal)));
		CHECK(wld.get<Position>(instance).x == doctest::Approx(1.0f));
		CHECK(wld.get<Position>(instance).y == doctest::Approx(2.0f));
		CHECK(wld.get<Position>(instance).z == doctest::Approx(3.0f));
	}
}

TEST_CASE("Prefab - instantiate_n does not copy prefab names") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	wld.name(prefabAnimal, "prefab_animal_bulk");

	cnt::darray<ecs::Entity> roots;
	wld.instantiate_n(prefabAnimal, 5, [&](ecs::Entity instance) {
		roots.push_back(instance);
	});

	CHECK(roots.size() == 5);
	CHECK(!wld.name(prefabAnimal).empty());
	CHECK(wld.name(prefabAnimal) == "prefab_animal_bulk");
	for (const auto instance: roots)
		CHECK(wld.name(instance).empty());
}

TEST_CASE("Instantiate_n - non-prefab parented fallback with CopyIter callbacks") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto animal = wld.add();
	wld.add<Position>(animal, {1, 2, 3});

	uint32_t hits = 0;
	uint32_t seen = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(6);
	wld.instantiate_n(animal, scene, 6, [&](ecs::CopyIter& it) {
		++hits;
		seen += it.size();

		auto entityView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			roots.push_back(entityView[i]);
			CHECK(wld.has(entityView[i], ecs::Pair(ecs::Parent, scene)));
			CHECK(posView[i].x == doctest::Approx(1.0f));
			CHECK(posView[i].y == doctest::Approx(2.0f));
			CHECK(posView[i].z == doctest::Approx(3.0f));
		}
	});

	CHECK(hits >= 1);
	CHECK(seen == 6);
	CHECK(roots.size() == 6);
	for (const auto instance: roots) {
		CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
		CHECK_FALSE(wld.has_direct(instance, ecs::Pair(ecs::Is, animal)));
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
	}
}

TEST_CASE("Instantiate_n - non-prefab parented fallback with entity callbacks after parenting") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto animal = wld.add();
	wld.add<Position>(animal, {7, 8, 9});

	uint32_t seen = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(5);
	wld.instantiate_n(animal, scene, 5, [&](ecs::Entity instance) {
		++seen;
		roots.push_back(instance);
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
		const auto& pos = wld.get<Position>(instance);
		CHECK(pos.x == doctest::Approx(7.0f));
		CHECK(pos.y == doctest::Approx(8.0f));
		CHECK(pos.z == doctest::Approx(9.0f));
	});

	CHECK(seen == 5);
	CHECK(roots.size() == 5);
	for (const auto instance: roots)
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
}

TEST_CASE("Observer - instantiate_n non-prefab parented fallback and Parent pair") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto animal = wld.add();
	wld.add<Position>(animal, {7, 8, 9});

	uint32_t hits = 0;
	uint32_t seen = 0;
	auto obs =
			wld.observer().event(ecs::ObserverEvent::OnAdd).all(ecs::Pair(ecs::Parent, scene)).on_each([&](ecs::Iter& it) {
				++hits;
				seen += it.size();

				auto entityView = it.view<ecs::Entity>();
				GAIA_EACH(it) {
					CHECK(wld.has(entityView[i], ecs::Pair(ecs::Parent, scene)));
				}
			});
	(void)obs;

	wld.instantiate_n(animal, scene, 5, [&](ecs::Entity instance) {
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
	});

	CHECK(hits >= 1);
	CHECK(seen == 5);
}

TEST_CASE("Observer - parented prefab instantiate matches Parent pair") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {1, 2, 3});

	uint32_t hits = 0;
	uint32_t seen = 0;
	auto obs = wld.observer() //
								 .event(ecs::ObserverEvent::OnAdd) //
								 .all(ecs::Pair(ecs::Parent, scene)) //
								 .on_each([&](ecs::Iter& it) {
									 ++hits;
									 seen += it.size();

									 auto entityView = it.view<ecs::Entity>();
									 GAIA_EACH(it) {
										 CHECK(wld.has(entityView[i], ecs::Pair(ecs::Parent, scene)));
									 }
								 });
	(void)obs;

	const auto instance = wld.instantiate(prefab, scene);
	CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
	CHECK(hits == 1);
	CHECK(seen == 1);
}

TEST_CASE("Prefab - instantiate_n CopyIter callbacks for spawned roots") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	wld.add<Position>(prefabAnimal, {1, 2, 3});

	uint32_t hits = 0;
	uint32_t seen = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(8);
	wld.instantiate_n(prefabAnimal, 8, [&](ecs::CopyIter& it) {
		++hits;
		seen += it.size();

		auto entityView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			roots.push_back(entityView[i]);
			CHECK(posView[i].x == doctest::Approx(1.0f));
			CHECK(posView[i].y == doctest::Approx(2.0f));
			CHECK(posView[i].z == doctest::Approx(3.0f));
		}
	});

	CHECK(hits >= 1);
	CHECK(seen == 8);
	CHECK(roots.size() == 8);
	for (const auto instance: roots) {
		CHECK_FALSE(wld.has_direct(instance, ecs::Prefab));
		CHECK(wld.has_direct(instance, ecs::Pair(ecs::Is, prefabAnimal)));
	}
}

TEST_CASE("Prefab - instantiate_n inherited component queries and local overrides") {
	TestWorld twld;

	const auto prefabAnimal = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefabAnimal, {5, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	cnt::darray<ecs::Entity> instances;
	wld.instantiate_n(prefabAnimal, 4, [&](ecs::Entity instance) {
		instances.push_back(instance);
	});

	CHECK(instances.size() == 4);

	uint32_t hits = 0;
	wld.query().all<Position&>().each([&](Position& pos) {
		++hits;
		pos.x += 2.0f;
	});

	CHECK(hits == 4);
	for (const auto instance: instances) {
		CHECK(wld.has_direct(instance, position));
		CHECK(wld.get<Position>(instance).x == doctest::Approx(7.0f));
	}
	CHECK(wld.get<Position>(prefabAnimal).x == doctest::Approx(5.0f));
}

TEST_CASE("Prefab - instantiate_n parented roots support CopyIter callbacks") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});

	uint32_t rootCount = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(4);
	wld.instantiate_n(rootPrefab, scene, 4, [&](ecs::CopyIter& it) {
		rootCount += it.size();
		auto entityView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			roots.push_back(entityView[i]);
			CHECK(posView[i].x == doctest::Approx(1.0f));
			CHECK(wld.has(entityView[i], ecs::Pair(ecs::Parent, scene)));
		}
	});

	CHECK(rootCount == 4);
	CHECK(roots.size() == 4);

	uint32_t childCount = 0;
	for (const auto instance: roots) {
		wld.sources(ecs::Parent, instance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::Parent, instance)));
		});
	}
	CHECK(childCount == 4);
}

TEST_CASE("Prefab - instantiate_n parented ChildOf roots support CopyIter callbacks") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();

	wld.child(childPrefab, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});

	uint32_t rootCount = 0;
	cnt::darray<ecs::Entity> roots;
	roots.reserve(4);
	wld.instantiate_n(rootPrefab, scene, 4, [&](ecs::CopyIter& it) {
		rootCount += it.size();
		auto entityView = it.view<ecs::Entity>();
		auto posView = it.view<Position>();
		GAIA_EACH(it) {
			const auto root = entityView[i];
			roots.push_back(root);
			CHECK(posView[i].x == doctest::Approx(1.0f));
			CHECK(wld.has(root, ecs::Pair(ecs::Parent, scene)));

			const auto child = wld.find_prefab_instance(root, childPrefab);
			CHECK(child != ecs::EntityBad);
			if (child == ecs::EntityBad)
				continue;

			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, root)));
			CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));
		}
	});

	CHECK(rootCount == 4);
	CHECK(roots.size() == 4);

	uint32_t childCount = 0;
	for (const auto instance: roots) {
		wld.sources(ecs::ChildOf, instance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, instance)));
		});
	}
	CHECK(childCount == 4);
}

TEST_CASE("Prefab - instantiate_n can parent each spawned subtree") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});

	cnt::darray<ecs::Entity> rootInstances;
	wld.instantiate_n(rootPrefab, scene, 3, [&](ecs::Entity instance) {
		rootInstances.push_back(instance);
	});

	CHECK(rootInstances.size() == 3);

	uint32_t childCount = 0;
	for (const auto instance: rootInstances) {
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
		wld.sources(ecs::Parent, instance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::Parent, instance)));
			CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));
		});
	}

	CHECK(childCount == 3);
}

TEST_CASE("Prefab - instantiate_n can parent each spawned ChildOf subtree") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();

	wld.child(childPrefab, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});

	cnt::darray<ecs::Entity> rootInstances;
	wld.instantiate_n(rootPrefab, scene, 3, [&](ecs::Entity instance) {
		rootInstances.push_back(instance);
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));

		const auto child = wld.find_prefab_instance(instance, childPrefab);
		CHECK(child != ecs::EntityBad);
		if (child == ecs::EntityBad)
			return;

		CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
		CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, instance)));
		CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));
	});

	CHECK(rootInstances.size() == 3);

	uint32_t childCount = 0;
	for (const auto instance: rootInstances) {
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
		wld.sources(ecs::ChildOf, instance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, instance)));
		});
	}

	CHECK(childCount == 3);
}

TEST_CASE("Prefab - instantiate_n can parent each spawned nested ChildOf subtree") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.child(childPrefab, rootPrefab);
	wld.child(leafPrefab, childPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	cnt::darray<ecs::Entity> rootInstances;
	wld.instantiate_n(rootPrefab, scene, 3, [&](ecs::Entity instance) {
		rootInstances.push_back(instance);
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));

		const auto child = wld.find_prefab_instance(instance, childPrefab);
		const auto leaf = wld.find_prefab_instance(instance, leafPrefab);
		CHECK(child != ecs::EntityBad);
		CHECK(leaf != ecs::EntityBad);
		if (child == ecs::EntityBad || leaf == ecs::EntityBad)
			return;

		CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
		CHECK(wld.has_direct(leaf, ecs::Pair(ecs::Is, leafPrefab)));
		CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, instance)));
		CHECK(wld.has(leaf, ecs::Pair(ecs::ChildOf, child)));
		CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));
		CHECK(wld.get<Position>(leaf).x == doctest::Approx(3.0f));
	});

	CHECK(rootInstances.size() == 3);

	uint32_t childCount = 0;
	uint32_t leafCount = 0;
	for (const auto instance: rootInstances) {
		CHECK(wld.has(instance, ecs::Pair(ecs::Parent, scene)));
		wld.sources(ecs::ChildOf, instance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, instance)));

			wld.sources(ecs::ChildOf, child, [&](ecs::Entity leaf) {
				++leafCount;
				CHECK(wld.has_direct(leaf, ecs::Pair(ecs::Is, leafPrefab)));
				CHECK(wld.has(leaf, ecs::Pair(ecs::ChildOf, child)));
			});
		});
	}

	CHECK(childCount == 3);
	CHECK(leafCount == 3);
}

TEST_CASE("Prefab - instantiate_n callbacks see spawned child subtrees") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});

	uint32_t entityCallbackRoots = 0;
	wld.instantiate_n(rootPrefab, 3, [&](ecs::Entity instance) {
		++entityCallbackRoots;

		uint32_t childCount = 0;
		wld.sources(ecs::Parent, instance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::Parent, instance)));
		});
		CHECK(childCount == 1);
	});
	CHECK(entityCallbackRoots == 3);

	uint32_t copyIterCallbackRoots = 0;
	wld.instantiate_n(rootPrefab, 4, [&](ecs::CopyIter& it) {
		copyIterCallbackRoots += it.size();
		auto entityView = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			uint32_t childCount = 0;
			wld.sources(ecs::Parent, entityView[i], [&](ecs::Entity child) {
				++childCount;
				CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
				CHECK(wld.has(child, ecs::Pair(ecs::Parent, entityView[i])));
			});
			CHECK(childCount == 1);
		}
	});
	CHECK(copyIterCallbackRoots == 4);
}

TEST_CASE("Prefab - instantiate_n CopyIter callbacks see spawned ChildOf subtrees") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();

	wld.child(childPrefab, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});

	uint32_t copyIterCallbackRoots = 0;
	wld.instantiate_n(rootPrefab, 4, [&](ecs::CopyIter& it) {
		copyIterCallbackRoots += it.size();
		auto entityView = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			const auto child = wld.find_prefab_instance(entityView[i], childPrefab);
			CHECK(child != ecs::EntityBad);
			if (child == ecs::EntityBad)
				continue;

			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.has(child, ecs::Pair(ecs::ChildOf, entityView[i])));
			CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));
		}
	});

	CHECK(copyIterCallbackRoots == 4);
}

TEST_CASE("Prefab - instantiate_n recurses nested prefab children") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.parent(leafPrefab, childPrefab);

	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	cnt::darray<ecs::Entity> roots;
	wld.instantiate_n(rootPrefab, 2, [&](ecs::Entity instance) {
		roots.push_back(instance);
	});

	CHECK(roots.size() == 2);

	uint32_t childCount = 0;
	uint32_t leafCount = 0;
	for (const auto rootInstance: roots) {
		wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
			++childCount;
			CHECK(wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)));
			CHECK(wld.get<Position>(child).x == doctest::Approx(2.0f));

			wld.sources(ecs::Parent, child, [&](ecs::Entity leaf) {
				++leafCount;
				CHECK(wld.has_direct(leaf, ecs::Pair(ecs::Is, leafPrefab)));
				CHECK(wld.get<Position>(leaf).x == doctest::Approx(3.0f));
			});
		});
	}

	CHECK(childCount == 2);
	CHECK(leafCount == 2);
}

TEST_CASE("Prefab - nested Parent prefab children can be found through shared source helpers") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	wld.parent(childPrefab, rootPrefab);
	wld.parent(leafPrefab, childPrefab);

	const auto rootInstance = wld.instantiate(rootPrefab);

	cnt::darray<ecs::Entity> children;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		children.push_back(child);
	});
	uint32_t visitedUntilStop = 0;
	wld.sources_if(ecs::Parent, rootInstance, [&](ecs::Entity) {
		++visitedUntilStop;
		return false;
	});

	CHECK(children.size() == 1);
	CHECK(visitedUntilStop == 1);
	if (children.size() == 1) {
		CHECK(wld.has_direct(children[0], ecs::Pair(ecs::Is, childPrefab)));

		cnt::darray<ecs::Entity> leaves;
		ecs::sources(wld, ecs::Parent, children[0], [&](ecs::Entity leaf) {
			leaves.push_back(leaf);
		});

		CHECK(leaves.size() == 1);
		if (leaves.size() == 1) {
			CHECK(wld.has_direct(leaves[0], ecs::Pair(ecs::Is, leafPrefab)));
			CHECK(wld.has(leaves[0], ecs::Pair(ecs::Parent, children[0])));
		}
	}

	cnt::darray<ecs::Entity> descendants;
	wld.sources_bfs(ecs::Parent, rootInstance, [&](ecs::Entity descendant) {
		descendants.push_back(descendant);
	});
	CHECK(descendants.size() == 2);

	const bool stoppedOnLeaf = ecs::sources_bfs_if(wld, ecs::Parent, rootInstance, [&](ecs::Entity descendant) {
		return wld.has_direct(descendant, ecs::Pair(ecs::Is, leafPrefab));
	});
	CHECK(stoppedOnLeaf);
}

TEST_CASE("Prefab - prefab instance lookup maps nested child prefabs under a spawned root") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();
	const auto missingPrefab = wld.prefab();

	wld.name(rootPrefab, "prefab_lookup_root");
	wld.name(childPrefab, "prefab_lookup_child");
	wld.name(leafPrefab, "prefab_lookup_leaf");
	wld.name(missingPrefab, "prefab_lookup_missing");

	wld.parent(childPrefab, rootPrefab);
	wld.parent(leafPrefab, childPrefab);

	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add<Position>(leafPrefab, {3, 0, 0});

	const auto returnedRoot = wld.instantiate(rootPrefab);
	const auto returnedChild = wld.find_prefab_instance(returnedRoot, childPrefab);
	const auto returnedLeaf = wld.find_prefab_instance(returnedRoot, leafPrefab);
	CHECK(returnedChild != ecs::EntityBad);
	CHECK(returnedLeaf != ecs::EntityBad);
	if (returnedChild != ecs::EntityBad && returnedLeaf != ecs::EntityBad) {
		uint32_t directChildCount = 0;
		wld.sources(ecs::Parent, returnedRoot, [&](ecs::Entity child) {
			++directChildCount;
			CHECK(child == returnedChild);
		});
		CHECK(directChildCount == 1);
		CHECK(wld.has(returnedLeaf, ecs::Pair(ecs::Parent, returnedChild)));
	}

	cnt::darray<ecs::Entity> roots;
	wld.instantiate_n(rootPrefab, 2, [&](ecs::CopyIter& it) {
		auto entityView = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			roots.push_back(entityView[i]);
		}
	});

	CHECK(roots.size() == 2);
	for (const auto rootInstance: roots) {
		const auto rootLookup = wld.find_prefab_instance(rootInstance, rootPrefab);
		const auto childInstance = wld.find_prefab_instance(rootInstance, childPrefab);
		const auto leafInstance = wld.find_prefab_instance(rootInstance, leafPrefab);

		CHECK(rootLookup == rootInstance);
		CHECK(childInstance != ecs::EntityBad);
		CHECK(leafInstance != ecs::EntityBad);
		CHECK(wld.find_prefab_instance(rootInstance, missingPrefab) == ecs::EntityBad);
		if (childInstance == ecs::EntityBad || leafInstance == ecs::EntityBad)
			continue;

		CHECK(wld.name(childInstance).empty());
		CHECK(wld.name(leafInstance).empty());
		CHECK(wld.has(childInstance, ecs::Pair(ecs::Parent, rootInstance)));
		CHECK(wld.has(leafInstance, ecs::Pair(ecs::Parent, childInstance)));
		CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Is, childPrefab)));
		CHECK(wld.has_direct(leafInstance, ecs::Pair(ecs::Is, leafPrefab)));
		CHECK(wld.get<Position>(childInstance).x == doctest::Approx(2.0f));
		CHECK(wld.get<Position>(leafInstance).x == doctest::Approx(3.0f));
	}
}

TEST_CASE("Prefab - instantiate_n with zero count does nothing") {
	TestWorld twld;

	const auto scene = wld.add();
	const auto prefabAnimal = wld.prefab();
	wld.add<Position>(prefabAnimal, {1.0f, 2.0f, 3.0f});

	uint32_t entityHits = 0;
	wld.instantiate_n(prefabAnimal, 0, [&](ecs::Entity) {
		++entityHits;
	});
	CHECK(entityHits == 0);

	uint32_t iterHits = 0;
	wld.instantiate_n(prefabAnimal, scene, 0, [&](ecs::CopyIter& it) {
		iterHits += it.size();
	});
	CHECK(iterHits == 0);

	auto q = wld.query().all<Position>();
	CHECK(q.count() == 0);
}

TEST_CASE("Prefab - sync adds missing copied data to existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);

	auto prefabBuilder = wld.build(prefab);
	prefabBuilder.add<Position>();
	prefabBuilder.commit();
	wld.set<Position>(prefab) = {1.0f, 2.0f, 3.0f};

	CHECK_FALSE(wld.has<Position>(instance));

	const auto changes = wld.sync(prefab);
	CHECK(changes == 1);
	CHECK(wld.has_direct(instance, wld.add<Position>().entity));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(1.0f));
	CHECK(wld.get<Position>(instance).y == doctest::Approx(2.0f));
	CHECK(wld.get<Position>(instance).z == doctest::Approx(3.0f));
}

TEST_CASE("Prefab - sync spawns missing prefab children on existing instances") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto rootInstance = wld.instantiate(rootPrefab);
	const auto childPrefab = wld.prefab();
	const auto leafPrefab = wld.prefab();

	auto childBuilder = wld.build(childPrefab);
	childBuilder.add<Position>();
	childBuilder.add(ecs::Pair(ecs::Parent, rootPrefab));
	childBuilder.commit();
	wld.set<Position>(childPrefab) = {2.0f, 0.0f, 0.0f};
	wld.parent(leafPrefab, childPrefab);
	wld.add<Scale>(leafPrefab, {3.0f, 0.0f, 0.0f});

	CHECK_FALSE(wld.has(rootInstance, ecs::Pair(ecs::Parent, rootPrefab)));

	const auto changes = wld.sync(rootPrefab);
	CHECK(changes == 1);

	cnt::darray<ecs::Entity> childInstances;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstances.push_back(child);
	});

	CHECK(childInstances.size() == 1);
	CHECK(wld.get<Position>(childInstances[0]).x == doctest::Approx(2.0f));
	CHECK(wld.find_prefab_instance(rootInstance, childPrefab) == childInstances[0]);
	const auto leafInstance = wld.find_prefab_instance(rootInstance, leafPrefab);
	CHECK(leafInstance != ecs::EntityBad);
	if (leafInstance != ecs::EntityBad) {
		CHECK(wld.has(leafInstance, ecs::Pair(ecs::Parent, childInstances[0])));
		CHECK(wld.get<Scale>(leafInstance).x == doctest::Approx(3.0f));
	}

	const auto changesAgain = wld.sync(rootPrefab);
	CHECK(changesAgain == 0);
}

TEST_CASE("Prefab - sync spawns missing ChildOf prefab children on existing instances") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto rootInstance = wld.instantiate(rootPrefab);
	const auto childPrefab = wld.prefab();

	wld.child(childPrefab, rootPrefab);
	wld.add<Position>(childPrefab, {4.0f, 0.0f, 0.0f});

	CHECK(wld.find_prefab_instance(rootInstance, childPrefab) == ecs::EntityBad);

	const auto changes = wld.sync(rootPrefab);
	CHECK(changes == 1);

	const auto childInstance = wld.find_prefab_instance(rootInstance, childPrefab);
	CHECK(childInstance != ecs::EntityBad);
	if (childInstance != ecs::EntityBad) {
		CHECK(wld.has(childInstance, ecs::Pair(ecs::ChildOf, rootInstance)));
		CHECK_FALSE(wld.has(childInstance, ecs::Pair(ecs::ChildOf, rootPrefab)));
		CHECK(wld.get<Position>(childInstance).x == doctest::Approx(4.0f));
	}

	const auto changesAgain = wld.sync(rootPrefab);
	CHECK(changesAgain == 0);
}

TEST_CASE("Prefab - sync recurses into existing child instances") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	{
		auto childBuilder = wld.build(childPrefab);
		childBuilder.add(ecs::Pair(ecs::Parent, rootPrefab));
		childBuilder.commit();
	}

	const auto rootInstance = wld.instantiate(rootPrefab);

	ecs::Entity childInstance = ecs::EntityBad;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstance = child;
	});
	CHECK(childInstance != ecs::EntityBad);
	if (childInstance == ecs::EntityBad)
		return;

	{
		auto childBuilder = wld.build(childPrefab);
		childBuilder.add<Scale>();
		childBuilder.commit();
	}
	wld.set<Scale>(childPrefab) = {1.0f, 0.5f, 0.25f};

	CHECK_FALSE(wld.has<Scale>(childInstance));

	const auto changes = wld.sync(rootPrefab);
	CHECK(changes == 1);
	CHECK(wld.has_direct(childInstance, wld.add<Scale>().entity));
	CHECK(wld.get<Scale>(childInstance).x == doctest::Approx(1.0f));
	CHECK(wld.get<Scale>(childInstance).y == doctest::Approx(0.5f));
	CHECK(wld.get<Scale>(childInstance).z == doctest::Approx(0.25f));
}

TEST_CASE("Prefab - removing inherited prefab data on existing instances") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.add<Position>(prefab, {1.0f, 2.0f, 3.0f});

	const auto instance = wld.instantiate(prefab);
	CHECK(wld.has<Position>(instance));

	wld.del<Position>(prefab);

	CHECK_FALSE(wld.has<Position>(instance));
	CHECK_FALSE(wld.has_direct(instance, position));
	CHECK(wld.sync(prefab) == 0);
	CHECK_FALSE(wld.has<Position>(instance));
}

TEST_CASE("Prefab - sync does not delete copied override data removed from prefab") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {4.0f, 5.0f, 6.0f});

	const auto instance = wld.instantiate(prefab);
	CHECK(wld.has_direct(instance, wld.add<Position>().entity));

	wld.del<Position>(prefab);

	CHECK(wld.has<Position>(instance));
	CHECK(wld.sync(prefab) == 0);
	CHECK(wld.has<Position>(instance));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(4.0f));
	CHECK(wld.get<Position>(instance).y == doctest::Approx(5.0f));
	CHECK(wld.get<Position>(instance).z == doctest::Approx(6.0f));
}

TEST_CASE("Prefab - sync does not delete existing child instances when prefab child is removed") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	wld.parent(childPrefab, rootPrefab);
	wld.add<Position>(childPrefab, {2.0f, 0.0f, 0.0f});

	const auto rootInstance = wld.instantiate(rootPrefab);

	cnt::darray<ecs::Entity> childInstances;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity child) {
		if (wld.has_direct(child, ecs::Pair(ecs::Is, childPrefab)))
			childInstances.push_back(child);
	});

	CHECK(childInstances.size() == 1);
	if (childInstances.size() != 1)
		return;
	const auto childInstance = childInstances[0];

	wld.del(childPrefab, ecs::Pair(ecs::Parent, rootPrefab));

	CHECK(wld.sync(rootPrefab) == 0);
	CHECK(wld.has(childInstance, ecs::Pair(ecs::Parent, rootInstance)));
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Is, childPrefab)));
	CHECK(wld.get<Position>(childInstance).x == doctest::Approx(2.0f));
}

TEST_CASE("Prefab - cached exact Is query ignores recycled prefab ids after delete") {
	TestWorld twld;

	const auto recycle_same_id = [&](ecs::Entity stale) {
		ecs::Entity recycled = ecs::EntityBad;
		for (uint32_t i = 0; i < 256 && recycled == ecs::EntityBad; ++i) {
			const auto candidate = wld.add();
			if (candidate.id() == stale.id())
				recycled = candidate;
		}
		return recycled;
	};

	const auto prefab = wld.prefab();
	const auto oldInstance = wld.instantiate(prefab);

	ecs::QueryTermOptions directOpts;
	directOpts.direct();

	auto q = wld.query().all(ecs::Pair(ecs::Is, prefab), directOpts);
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {oldInstance});

	wld.del(prefab);
	twld.update();

	q.match_all(info);
	CHECK(q.count() == 0);

	const auto recycled = recycle_same_id(prefab);
	CHECK(recycled != ecs::EntityBad);
	if (recycled == ecs::EntityBad)
		return;
	CHECK(recycled.gen() != prefab.gen());

	wld.add(recycled, ecs::Prefab);

	q.match_all(info);
	CHECK(q.count() == 0);

	auto qFresh = wld.query().all(ecs::Pair(ecs::Is, recycled), directOpts);
	CHECK(qFresh.count() == 0);

	const auto newInstance = wld.instantiate(recycled);

	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {newInstance});

	CHECK(qFresh.count() == 1);
	expect_exact_entities(qFresh, {newInstance});
	CHECK_FALSE(wld.has_direct(oldInstance, ecs::Pair(ecs::Is, recycled)));
}

TEST_CASE("Prefab - inherited component query ignores recycled prefab ids after delete") {
	TestWorld twld;

	const auto recycle_same_id = [&](ecs::Entity stale) {
		ecs::Entity recycled = ecs::EntityBad;
		for (uint32_t i = 0; i < 256 && recycled == ecs::EntityBad; ++i) {
			const auto candidate = wld.add();
			if (candidate.id() == stale.id())
				recycled = candidate;
		}
		return recycled;
	};

	const auto position = wld.add<Position>().entity;
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {1.0f, 2.0f, 3.0f});

	const auto oldInstance = wld.instantiate(prefab);

	auto q = wld.query().all<Position>();
	auto& info = q.fetch();
	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {oldInstance});

	wld.del(prefab);
	twld.update();

	q.match_all(info);
	CHECK(q.count() == 0);
	CHECK_FALSE(wld.has<Position>(oldInstance));

	const auto recycled = recycle_same_id(prefab);
	CHECK(recycled != ecs::EntityBad);
	if (recycled == ecs::EntityBad)
		return;
	CHECK(recycled.gen() != prefab.gen());

	wld.add(recycled, ecs::Prefab);
	wld.add<Position>(recycled, {7.0f, 8.0f, 9.0f});

	q.match_all(info);
	CHECK(q.count() == 0);
	CHECK_FALSE(wld.has<Position>(oldInstance));

	const auto newInstance = wld.instantiate(recycled);

	q.match_all(info);
	CHECK(q.count() == 1);
	expect_exact_entities(q, {newInstance});
	CHECK_FALSE(wld.has<Position>(oldInstance));
	CHECK(wld.get<Position>(newInstance).x == doctest::Approx(7.0f));
	CHECK(wld.get<Position>(newInstance).y == doctest::Approx(8.0f));
	CHECK(wld.get<Position>(newInstance).z == doctest::Approx(9.0f));
}

TEST_CASE("Prefab - instantiate with non-prefab Parent children excluded") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto plainChild = wld.add();

	wld.parent(plainChild, rootPrefab);
	wld.add<Position>(rootPrefab, {1, 0, 0});
	wld.add<Position>(plainChild, {2, 0, 0});

	const auto rootInstance = wld.instantiate(rootPrefab);

	uint32_t childCount = 0;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity) {
		++childCount;
	});

	CHECK(childCount == 0);
	CHECK(wld.get<Position>(rootInstance).x == doctest::Approx(1.0f));
}

TEST_CASE("Prefab - child instantiation respects DontInherit policy") {
	TestWorld twld;

	const auto rootPrefab = wld.prefab();
	const auto childPrefab = wld.prefab();
	const auto position = wld.add<Position>().entity;

	wld.parent(childPrefab, rootPrefab);
	wld.add<Scale>(rootPrefab, {1, 0, 0});
	wld.add<Position>(childPrefab, {2, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::DontInherit));

	const auto rootInstance = wld.instantiate(rootPrefab);

	ecs::Entity childInstance = ecs::EntityBad;
	wld.sources(ecs::Parent, rootInstance, [&](ecs::Entity entity) {
		childInstance = entity;
	});

	CHECK(childInstance != ecs::EntityBad);
	CHECK_FALSE(wld.has<Position>(childInstance));
	CHECK(wld.has_direct(childInstance, ecs::Pair(ecs::Is, childPrefab)));
}
