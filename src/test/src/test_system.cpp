#include "test_common.h"

TEST_CASE("System - simple") {
	uint32_t sys1_cnt = 0;
	uint32_t sys2_cnt = 0;
	uint32_t sys3_cnt = 0;
	bool sys3_run_before_sys1 = false;
	bool sys3_run_before_sys2 = false;
	TestWorld twld;

	constexpr uint32_t N = 10;
	{
		auto e = wld.add();
		wld.add<Position>(e, {0, 100, 0});
		wld.add<Acceleration>(e, {1, 0, 0});
		GAIA_FOR(N - 1) {
			[[maybe_unused]] auto newEntity = wld.copy(e);
		}
	}

	auto testRun = [&]() {
		GAIA_FOR(100) {
			sys3_run_before_sys1 = false;
			sys3_run_before_sys2 = false;
			wld.update();
			CHECK(sys1_cnt == N);
			CHECK(sys2_cnt == N);
			CHECK(sys3_cnt == N);
			sys1_cnt = 0;
			sys2_cnt = 0;
			sys3_cnt = 0;
		}
	};

	// Our systems
	auto sys1 = wld.system()
									//.name("sys1")
									.all<Position>()
									.all<Acceleration>() //
									.on_each([&](Position, Acceleration) {
										if (sys1_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys1 = true;
										++sys1_cnt;
									});
	auto sys2 = wld.system()
									//.name("sys2")
									.all<Position>() //
									.on_each([&](ecs::Iter& it) {
										if (sys2_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys2 = true;
										GAIA_EACH(it)++ sys2_cnt;
									});
	auto sys3 = wld.system()
									// TODO: Using names for the systems can break ordering after rebulid.
									//       Most likely an undefined behavior somewhere (maybe partial sort on systems?).
									//       Find out what is wrong.
									//.name("sys3")
									.all<Acceleration>() //
									.on_each([&](ecs::Iter& it) {
										GAIA_EACH(it)++ sys3_cnt;
									});

	testRun();

	// Make sure to execute sys2 before sys1
	wld.add(sys1.entity(), {ecs::DependsOn, sys3.entity()});
	wld.add(sys2.entity(), {ecs::DependsOn, sys3.entity()});

	testRun();

	CHECK(sys3_run_before_sys1);
	CHECK(sys3_run_before_sys2);
}

TEST_CASE("System - dependency BFS order") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto make_sys = [&](char id) {
		return wld.system().all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
	};

	auto sysRoot = make_sys('R');
	auto sysA = make_sys('A');
	auto sysC = make_sys('C');
	auto sysB = make_sys('B');

	// Dependency graph:
	//   R -> A -> C
	//   R -> B
	// BFS levels should execute: R, A+B, C.
	wld.add(sysA.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysB.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysC.entity(), {ecs::DependsOn, sysA.entity()});

	wld.update();

	CHECK(order.size() == 4);
	if (order.size() == 4) {
		CHECK(order[0] == 'R');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'B');
		CHECK(order[3] == 'C');
	}
}

TEST_CASE("System - DependsOn respects deepest dependency chain") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto make_sys = [&](char id) {
		return wld.system().all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
	};

	auto sysRoot = make_sys('R');
	auto sysA = make_sys('A');
	auto sysB = make_sys('B');
	auto sysC = make_sys('C');

	wld.add(sysA.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysB.entity(), {ecs::DependsOn, sysA.entity()});
	wld.add(sysC.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysC.entity(), {ecs::DependsOn, sysB.entity()});

	wld.update();

	CHECK(order.size() == 4);
	if (order.size() == 4) {
		CHECK(order[0] == 'R');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'B');
		CHECK(order[3] == 'C');
	}
}

TEST_CASE("System - DependsOn updates after dependency rewiring") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto make_sys = [&](char id) {
		return wld.system().all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
	};

	auto sysRoot = make_sys('R');
	auto sysA = make_sys('A');
	auto sysB = make_sys('B');
	auto sysC = make_sys('C');

	wld.add(sysA.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysB.entity(), {ecs::DependsOn, sysA.entity()});
	wld.add(sysC.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysC.entity(), {ecs::DependsOn, sysB.entity()});

	wld.update();

	CHECK(order.size() == 4);
	if (order.size() == 4) {
		CHECK(order[0] == 'R');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'B');
		CHECK(order[3] == 'C');
	}

	wld.del(sysC.entity(), {ecs::DependsOn, sysB.entity()});
	order.clear();
	wld.update();

	CHECK(order.size() == 4);
	if (order.size() == 4) {
		CHECK(order[0] == 'R');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'C');
		CHECK(order[3] == 'B');
	}
}

TEST_CASE("World - teardown removes runtime callbacks without executing them") {
	ecs::World world;
	uint32_t sysCnt = 0;
	uint32_t obsCnt = 0;

	const auto systemEntity = world.system()
																.all<Position>()
																.on_each([&](Position) {
																	++sysCnt;
																})
																.entity();

	const auto observerEntity = world.observer()
																	.event(ecs::ObserverEvent::OnAdd)
																	.all<Position>()
																	.on_each([&](ecs::Iter& it) {
																		obsCnt += (uint32_t)it.size();
																	})
																	.entity();

	const auto entityA = world.add();
	world.add<Position>(entityA);

	CHECK(world.valid(systemEntity));
	CHECK(world.valid(observerEntity));
	CHECK(obsCnt == 1);
	CHECK(sysCnt == 0);

	world.teardown();
	world.teardown();

	const auto entityB = world.add();
	world.add<Position>(entityB);
	world.update();

	CHECK(sysCnt == 0);
	CHECK(obsCnt == 1);
}

TEST_CASE("System - depth_order query order") {
	cnt::darr<ecs::Entity> order;
	TestWorld twld;

	auto root = wld.add();
	auto childA = wld.add();
	auto childB = wld.add();
	auto grandChild = wld.add();

	wld.child(childA, root);
	wld.child(childB, root);
	wld.child(grandChild, childA);

	wld.add<Position>(root, {0, 0, 0});
	wld.add<Position>(childA, {0, 0, 0});
	wld.add<Position>(childB, {0, 0, 0});
	wld.add<Position>(grandChild, {0, 0, 0});

	wld.system().all<Position>().depth_order(ecs::ChildOf).on_each([&](ecs::Entity entity, Position) {
		order.push_back(entity);
	});

	wld.update();

	CHECK(order.size() == 4);
	CHECK(order[0] == root);
	const bool secondIsChild = order[1] == childA || order[1] == childB;
	const bool thirdIsChild = order[2] == childA || order[2] == childB;
	CHECK(secondIsChild);
	CHECK(thirdIsChild);
	CHECK(order[1] != order[2]);
	CHECK(order[3] == grandChild);
}

TEST_CASE("System - world teardown drops cached query tracking before chunk destruction") {
	uint32_t runs = 0;

	{
		ecs::World world;

		auto entity = world.add();
		world.add<Position>(entity, {1, 2, 3});

		auto sysA = world.system().all<Position>().on_each([&](const Position&) {
			++runs;
		});
		auto sysB = world.system().all<Position>().on_each([](const Position&) {});

		world.add(sysB.entity(), {ecs::DependsOn, sysA.entity()});
		world.update();

		CHECK(runs == 1);
	}

	CHECK(runs == 1);
}

TEST_CASE("System - exec mode") {
	const uint32_t N = 10'000;

	std::mutex mtx;
	const ecs::Chunk* pPrevChunk = nullptr;
	uint32_t cnt0{};
	uint32_t cntChunks0{};
	uint32_t cnt1{};
	uint32_t cntChunks1{};

	TestWorld twld;

	auto create = [&]() {
		wld.build(wld.add()).add<Position>();
	};
	GAIA_FOR(N) create();

	auto s0 = wld.system() //
								.all<Position>() //
								.on_each([&](ecs::Iter& iter) {
									cnt0 += iter.size();
									cntChunks0 += (iter.chunk() != pPrevChunk);
									pPrevChunk = iter.chunk();
								})
								.mode(ecs::QueryExecType::Default);
	auto s1 = wld.system() //
								.all<Position>() //
								.on_each([&](ecs::Iter& iter) {
									std::scoped_lock lock(mtx);
									cnt1 += iter.size();
									cntChunks1 += (iter.chunk() != pPrevChunk);
									pPrevChunk = iter.chunk();
								});

	s0.exec();
	CHECK(cnt0 == N);
	CHECK(cntChunks0 > 1);

	pPrevChunk = nullptr;
	s1.mode(ecs::QueryExecType::Parallel).exec();
	CHECK(cnt0 == cnt1);
	CHECK(cntChunks0 == cntChunks1);

	pPrevChunk = nullptr;
	cnt1 = 0;
	cntChunks1 = 0;
	s1.mode(ecs::QueryExecType::ParallelEff).exec();
	CHECK(cnt0 == cnt1);
	CHECK(cntChunks0 == cntChunks1);

	pPrevChunk = nullptr;
	cnt1 = 0;
	cntChunks1 = 0;
	s1.mode(ecs::QueryExecType::ParallelPerf).exec();
	CHECK(cnt0 == cnt1);
	CHECK(cntChunks0 == cntChunks1);
}

TEST_CASE("System - is sugar matches semantic and direct Is terms") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	uint32_t semanticHits = 0;
	uint32_t directHits = 0;

	auto sysSemantic = wld.system().is(animal).on_each([&](ecs::Iter& it) {
		semanticHits += it.size();
	});

	auto sysDirect = wld.system().is(animal, ecs::QueryTermOptions{}.direct()).on_each([&](ecs::Iter& it) {
		directHits += it.size();
	});

	sysSemantic.exec();
	sysDirect.exec();

	CHECK(semanticHits == 3);
	CHECK(directHits == 1);
}

TEST_CASE("System - in sugar matches descendants but excludes the base entity") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	uint32_t hits = 0;

	auto sys = wld.system().in(animal).on_each([&](ecs::Iter& it) {
		hits += it.size();
	});

	sys.exec();

	CHECK(hits == 2);
}

TEST_CASE("System - prefabs are excluded by default and can be matched explicitly") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto entity = wld.add();

	wld.add<Position>(prefab, {1, 0, 0});
	wld.add<Position>(entity, {2, 0, 0});

	uint32_t defaultHits = 0;
	uint32_t prefabHits = 0;

	auto sysDefault = wld.system().all<Position>().on_each([&](ecs::Iter& it) {
		defaultHits += it.size();
	});
	auto sysMatchPrefab = wld.system().all<Position>().match_prefab().on_each([&](ecs::Iter& it) {
		prefabHits += it.size();
	});

	sysDefault.exec();
	sysMatchPrefab.exec();

	CHECK(defaultHits == 1);
	CHECK(prefabHits == 2);
}

TEST_CASE("System - inherited prefab component query writes local overrides") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	auto sys = wld.system().all<Position&>().on_each([&](Position& pos) {
		++hits;
		pos.x += 2.0f;
	});

	sys.exec();

	CHECK(hits == 1);
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(6.0f));
	CHECK(wld.get<Position>(prefab).x == doctest::Approx(4.0f));
}

TEST_CASE("System - inherited Is component query writes local overrides") {
	TestWorld twld;

	const auto animal = wld.add();
	const auto rabbit = wld.add();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(animal, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
	wld.as(rabbit, animal);

	uint32_t hits = 0;
	auto sys = wld.system().all<Position&>().on_each([&](Position& pos) {
		++hits;
		pos.x += 2.0f;
	});

	sys.exec();

	CHECK(hits == 2);
	CHECK(wld.has_direct(rabbit, position));
	CHECK(wld.get<Position>(animal).x == doctest::Approx(6.0f));
	CHECK(wld.get<Position>(rabbit).x == doctest::Approx(8.0f));
}

TEST_CASE("System - inherited prefab sparse component query writes local overrides") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<PositionSparse>().entity;
	wld.add<PositionSparse>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefab);

	uint32_t hits = 0;
	auto sys = wld.system().all<PositionSparse&>().on_each([&](PositionSparse& pos) {
		++hits;
		pos.x += 2.0f;
	});

	sys.exec();

	CHECK(hits == 1);
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<PositionSparse>(instance).x == doctest::Approx(6.0f));
	CHECK(wld.get<PositionSparse>(prefab).x == doctest::Approx(4.0f));
}

TEST_CASE("System - inherited prefab Iter query term-indexed access") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<Position>().entity;
	wld.add<Position>(prefab, {4, 0, 0});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefab);

	float xRead = 0.0f;
	auto sysRead = wld.system().all<Position>().on_each([&](ecs::Iter& it) {
		auto posView = it.view_any<Position>(1);
		GAIA_EACH(it) {
			xRead += posView[i].x;
		}
	});
	sysRead.exec();

	auto sysWrite = wld.system().all<Position&>().on_each([&](ecs::Iter& it) {
		auto posView = it.view_mut_any<Position>(1);
		GAIA_EACH(it) {
			posView[i].x += 3.0f;
		}
	});
	sysWrite.exec();

	CHECK(xRead == doctest::Approx(4.0f));
	CHECK(wld.has_direct(instance, position));
	CHECK(wld.get<Position>(instance).x == doctest::Approx(7.0f));
	CHECK(wld.get<Position>(prefab).x == doctest::Approx(4.0f));
}

TEST_CASE("System - inherited prefab Iter SoA query term-indexed access") {
	TestWorld twld;

	const auto prefab = wld.prefab();
	const auto position = wld.add<PositionSoA>().entity;
	wld.add<PositionSoA>(prefab, {4, 5, 6});
	wld.add(position, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));

	const auto instance = wld.instantiate(prefab);

	float xRead = 0.0f;
	float zRead = 0.0f;
	auto sysRead = wld.system().all<PositionSoA>().on_each([&](ecs::Iter& it) {
		auto posView = it.view_any<PositionSoA>(1);
		auto xs = posView.template get<0>();
		auto zs = posView.template get<2>();
		GAIA_EACH(it) {
			xRead += xs[i];
			zRead += zs[i];
		}
	});
	sysRead.exec();

	auto sysWrite = wld.system().all<PositionSoA&>().on_each([&](ecs::Iter& it) {
		auto posView = it.view_mut_any<PositionSoA>(1);
		auto xs = posView.template set<0>();
		auto zs = posView.template set<2>();
		GAIA_EACH(it) {
			xs[i] = xs[i] + 2.0f;
			zs[i] = zs[i] + 3.0f;
		}
	});
	sysWrite.exec();

	CHECK(xRead == doctest::Approx(4.0f));
	CHECK(zRead == doctest::Approx(6.0f));

	CHECK(wld.has_direct(instance, position));
	const auto pos = wld.get<PositionSoA>(instance);
	CHECK(pos.x == doctest::Approx(6.0f));
	CHECK(pos.y == doctest::Approx(5.0f));
	CHECK(pos.z == doctest::Approx(9.0f));

	const auto prefabPos = wld.get<PositionSoA>(prefab);
	CHECK(prefabPos.x == doctest::Approx(4.0f));
	CHECK(prefabPos.y == doctest::Approx(5.0f));
	CHECK(prefabPos.z == doctest::Approx(6.0f));
}

TEST_CASE("System - typed is query") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {4, 0, 0});
	wld.add<Position>(mammal, {1, 0, 0});
	wld.add<Position>(rabbit, {2, 0, 0});

	uint32_t semanticHits = 0;
	float semanticX = 0.0f;
	uint32_t directHits = 0;
	float directX = 0.0f;

	auto sysSemantic = wld.system().is(animal).all<Position>().on_each([&](const Position& pos) {
		++semanticHits;
		semanticX += pos.x;
	});

	auto sysDirect =
			wld.system().is(animal, ecs::QueryTermOptions{}.direct()).all<Position>().on_each([&](const Position& pos) {
				++directHits;
				directX += pos.x;
			});

	sysSemantic.exec();
	sysDirect.exec();

	CHECK(semanticHits == 3);
	CHECK(semanticX == doctest::Approx(7.0f));
	CHECK(directHits == 1);
	CHECK(directX == doctest::Approx(1.0f));
}

TEST_CASE("System - Iter is query term-indexed component access") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {4, 0, 0});
	wld.add<Position>(mammal, {1, 0, 0});
	wld.add<Position>(rabbit, {2, 0, 0});

	float semanticX = 0.0f;
	float directX = 0.0f;

	auto sysSemantic = wld.system().is(animal).all<Position>().on_each([&](ecs::Iter& it) {
		auto posView = it.view_any<Position>(1);
		GAIA_EACH(it) {
			semanticX += posView[i].x;
		}
	});

	auto sysDirect =
			wld.system().is(animal, ecs::QueryTermOptions{}.direct()).all<Position>().on_each([&](ecs::Iter& it) {
				auto posView = it.view<Position>(1);
				GAIA_EACH(it) {
					directX += posView[i].x;
				}
			});

	sysSemantic.exec();
	sysDirect.exec();

	CHECK(semanticX == doctest::Approx(7.0f));
	CHECK(directX == doctest::Approx(1.0f));
}

TEST_CASE("System - DependsOn and semantic Is queries") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {4, 0, 0});
	wld.add<Position>(mammal, {1, 0, 0});
	wld.add<Position>(rabbit, {2, 0, 0});

	uint32_t semanticHits = 0;
	bool directRanTooEarly = false;
	float directObservedX = 0.0f;

	auto sysSemantic = wld.system().is(animal).all<Position&>().on_each([&](Position& pos) {
		++semanticHits;
		pos.x += 10.0f;
	});

	auto sysDirect =
			wld.system().is(animal, ecs::QueryTermOptions{}.direct()).all<Position>().on_each([&](const Position& pos) {
				if (semanticHits != 3)
					directRanTooEarly = true;
				directObservedX += pos.x;
			});

	wld.add(sysDirect.entity(), {ecs::DependsOn, sysSemantic.entity()});
	wld.update();

	CHECK_FALSE(directRanTooEarly);
	CHECK(semanticHits == 3);
	CHECK(directObservedX == doctest::Approx(11.0f));
}

TEST_CASE("System - nested same-world Is query matcher scratch") {
	TestWorld twld;

	auto animal = wld.add();
	auto mammal = wld.add();
	auto rabbit = wld.add();

	wld.as(mammal, animal);
	wld.as(rabbit, mammal);

	wld.add<Position>(animal, {1, 0, 0});
	wld.add<Position>(mammal, {2, 0, 0});
	wld.add<Position>(rabbit, {3, 0, 0});

	uint32_t outerHits = 0;
	uint32_t nestedHits = 0;
	float nestedSum = 0.0f;

	wld.system().all<Position>().on_each([&](const Position&) {
		++outerHits;

		auto qNested = wld.query().all<Position>().is(animal);
		qNested.each([&](const Position& pos) {
			++nestedHits;
			nestedSum += pos.x;
		});
	});

	wld.update();

	CHECK(outerHits == 3);
	CHECK(nestedHits == 9);
	CHECK(nestedSum == doctest::Approx(18.0f));
}

TEST_CASE("System - deep semantic Is direct exec") {
	TestWorld twld;

	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
		}
		return root;
	};

	auto root = make_is_fanout(wld, 1024, 8);
	uint64_t sum = 0;

	auto sys = wld.system().all<Position>().is(root).on_each([&](const Position& pos) {
		sum += (uint64_t)(pos.x + pos.y + pos.z);
	});

	sys.exec();

	CHECK(sum == 8437760ULL);
}

TEST_CASE("System - deep semantic Is after prior systems query each") {
	TestWorld twld;

	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
		}
		return root;
	};

	auto root = make_is_fanout(wld, 1024, 8);
	uint64_t sum = 0;

	auto sys = wld.system().all<Position>().is(root).on_each([&](const Position& pos) {
		sum += (uint64_t)(pos.x + pos.y + pos.z);
	});

	wld.query().all(ecs::System).each([&](ecs::Entity) {});
	sys.exec();

	CHECK(sum == 8437760ULL);
}

TEST_CASE("Query - deep semantic Is after prior systems query each") {
	TestWorld twld;

	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
		}
		return root;
	};

	auto root = make_is_fanout(wld, 1024, 8);
	uint64_t sum = 0;

	auto sys = wld.system().all<Position>().is(root).on_each([&](const Position&) {});
	(void)sys;

	wld.query().all(ecs::System).each([&](ecs::Entity) {});

	auto q = wld.query().all<Position>().is(root);
	q.each([&](const Position& pos) {
		sum += (uint64_t)(pos.x + pos.y + pos.z);
	});

	CHECK(sum == 8437760ULL);
}

TEST_CASE("System - deep semantic Is after prior direct Is rematch in another world") {
	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
		}
		return root;
	};

	{
		TestWorld twld;
		auto root = make_is_fanout(wld, 1024, 8);
		auto q = wld.query().all<Position>().is(root, ecs::QueryTermOptions{}.direct());
		auto& qi = q.fetch();
		q.match_all(qi);
		CHECK(qi.cache_archetype_view().size() == 1);
	}

	{
		TestWorld twld;
		auto root = make_is_fanout(wld, 1024, 8);
		uint64_t sum = 0;

		wld.system().all<Position>().is(root).on_each([&](const Position& pos) {
			sum += (uint64_t)(pos.x + pos.y + pos.z);
		});

		wld.update();

		CHECK(sum == 8437760ULL);
	}
}

TEST_CASE("Observer - deep semantic Is matches_any after prior systems query each") {
	TestWorld twld;

	auto make_is_fanout = [](ecs::World& world, uint32_t branches, uint32_t depth, cnt::darr<ecs::Entity>& leaves) {
		auto root = world.add();
		for (uint32_t i = 0; i < branches; ++i) {
			auto curr = root;
			for (uint32_t j = 0; j < depth; ++j) {
				auto next = world.add();
				world.add(next, ecs::Pair(ecs::Is, curr));
				world.add<Position>(next, {(float)i, (float)j, (float)(i + j)});
				curr = next;
			}
			leaves.push_back(curr);
		}
		return root;
	};

	cnt::darr<ecs::Entity> leaves;
	auto root = make_is_fanout(wld, 512, 8, leaves);
	auto observerEntity = wld.observer().event(ecs::ObserverEvent::OnAdd).is(root).on_each([](ecs::Iter&) {}).entity();

	wld.query().all(ecs::System).each([&](ecs::Entity) {});

	auto& observerData = wld.observers().data(observerEntity);
	auto& observerQueryInfo = observerData.query.fetch();

	uint32_t matches = 0;
	for (const auto leaf: leaves) {
		const auto& ec = wld.fetch(leaf);
		matches += (uint32_t)observerData.query.matches_any(observerQueryInfo, *ec.pArchetype, ecs::EntitySpan{&leaf, 1});
	}

	CHECK(matches == leaves.size());
}

TEST_CASE("System - deep hierarchy skips disabled subtrees while preserving local enabled bits") {
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto grpA = wld.add();
	const auto subGrpA1 = wld.add();
	const auto subGrpA2 = wld.add();
	const auto grpB = wld.add();
	const auto subGrpB = wld.add();
	const auto grpC = wld.add();
	const auto subGrpC = wld.add();

	wld.child(subGrpA1, grpA);
	wld.child(subGrpA2, grpA);
	wld.child(subGrpB, grpB);
	wld.child(subGrpC, grpC);

	uint32_t hits[9] = {};

	auto make_sys = [&](uint32_t idx, ecs::Entity parent) {
		auto sys = wld.system().all<Position>().on_each([&, idx](Position) {
			++hits[idx];
		});
		wld.child(sys.entity(), parent);
		return sys.entity();
	};

	const auto sys1 = make_sys(0, subGrpA1);
	const auto sys2 = make_sys(1, subGrpA1);
	const auto sys3 = make_sys(2, subGrpA1);
	const auto sys4 = make_sys(3, subGrpA2);
	const auto sys5A = make_sys(4, subGrpA2);
	const auto sys5B = make_sys(5, subGrpB);
	const auto sys6 = make_sys(6, subGrpB);
	const auto sys10 = make_sys(7, subGrpC);
	const auto sys11 = make_sys(8, subGrpC);

	auto clear_hits = [&]() {
		GAIA_FOR(9) {
			hits[i] = 0;
		}
	};

	auto check_hits = [&](const bool expected[9]) {
		GAIA_FOR(9) {
			CHECK(hits[i] == (expected[i] ? 1u : 0u));
		}
	};

	const bool allEnabled[9] = {true, true, true, true, true, true, true, true, true};
	const bool subGrpA1Disabled[9] = {false, false, false, true, true, true, true, true, true};
	const bool subGrpCDisabled[9] = {true, true, true, true, true, true, true, false, false};
	const bool grpCDisabled[9] = {true, true, true, true, true, true, true, false, false};
	const bool sys10Disabled[9] = {true, true, true, true, true, true, true, false, true};
	const bool grpADisabled[9] = {false, false, false, false, false, true, true, true, true};

	clear_hits();
	wld.update();
	check_hits(allEnabled);

	wld.enable(subGrpA1, false);
	CHECK_FALSE(wld.enabled(subGrpA1));
	CHECK(wld.enabled(sys1));
	CHECK(wld.enabled(sys2));
	CHECK(wld.enabled(sys3));
	clear_hits();
	wld.update();
	check_hits(subGrpA1Disabled);

	wld.enable(subGrpA1, true);
	wld.enable(subGrpC, false);
	CHECK_FALSE(wld.enabled(subGrpC));
	CHECK(wld.enabled(sys10));
	CHECK(wld.enabled(sys11));
	clear_hits();
	wld.update();
	check_hits(subGrpCDisabled);

	wld.enable(subGrpC, true);
	wld.enable(grpC, false);
	CHECK_FALSE(wld.enabled(grpC));
	CHECK(wld.enabled(subGrpC));
	CHECK(wld.enabled(sys10));
	CHECK(wld.enabled(sys11));
	clear_hits();
	wld.update();
	check_hits(grpCDisabled);

	wld.enable(grpC, true);
	wld.enable(sys10, false);
	CHECK_FALSE(wld.enabled(sys10));
	CHECK(wld.enabled(sys11));
	clear_hits();
	wld.update();
	check_hits(sys10Disabled);

	wld.enable(sys10, true);
	wld.enable(grpA, false);
	CHECK_FALSE(wld.enabled(grpA));
	CHECK(wld.enabled(subGrpA1));
	CHECK(wld.enabled(subGrpA2));
	CHECK(wld.enabled(sys1));
	CHECK(wld.enabled(sys4));
	clear_hits();
	wld.update();
	check_hits(grpADisabled);

	(void)sys1;
	(void)sys2;
	(void)sys3;
	(void)sys4;
	(void)sys5A;
	(void)sys5B;
	(void)sys6;
	(void)sys10;
	(void)sys11;
}

TEST_CASE("System - cached direct-source query ignores recycled source ids") {
	TestWorld twld;

	const auto source = wld.add();
	wld.add<Acceleration>(source, {1.0f, 2.0f, 3.0f});

	const auto entity = wld.add();
	wld.add<Position>(entity, {4.0f, 5.0f, 6.0f});

	uint32_t hits = 0;
	cnt::darr<ecs::Entity> matched;
	wld.system().all<Position>().all<Acceleration>(ecs::QueryTermOptions{}.src(source)).on_each([&](ecs::Iter& it) {
		auto entities = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			++hits;
			matched.push_back(entities[i]);
		}
	});

	wld.update();
	CHECK(hits == 1);
	CHECK(matched.size() == 1);
	if (matched.size() == 1)
		CHECK(matched[0] == entity);

	hits = 0;
	matched.clear();

	wld.del(source);
	wld.update();
	CHECK(hits == 0);
	CHECK(matched.empty());

	ecs::Entity recycled = ecs::EntityBad;
	for (uint32_t i = 0; i < 256 && recycled == ecs::EntityBad; ++i) {
		const auto candidate = wld.add();
		if (candidate.id() == source.id())
			recycled = candidate;
	}
	CHECK(recycled != ecs::EntityBad);
	if (recycled != ecs::EntityBad)
		CHECK(recycled.gen() != source.gen());
	if (recycled != ecs::EntityBad)
		wld.add<Acceleration>(recycled, {7.0f, 8.0f, 9.0f});

	hits = 0;
	matched.clear();
	wld.update();
	CHECK(hits == 0);
	CHECK(matched.empty());
}

TEST_CASE("System - direct source or term tracks source changes") {
	TestWorld twld;

	const auto source = wld.add();
	const auto entity = wld.add();
	wld.add<Position>(entity, {1.0f, 2.0f, 3.0f});

	uint32_t hits = 0;
	cnt::darr<ecs::Entity> matched;
	wld.system().all<Position>().or_<Acceleration>(ecs::QueryTermOptions{}.src(source)).on_each([&](ecs::Iter& it) {
		auto entities = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			++hits;
			matched.push_back(entities[i]);
		}
	});

	wld.update();
	CHECK(hits == 0);
	CHECK(matched.empty());

	wld.add<Acceleration>(source, {4.0f, 5.0f, 6.0f});
	wld.update();
	CHECK(hits == 1);
	CHECK(matched.size() == 1);
	if (matched.size() == 1)
		CHECK(matched[0] == entity);

	hits = 0;
	matched.clear();
	wld.del<Acceleration>(source);
	wld.update();
	CHECK(hits == 0);
	CHECK(matched.empty());
}

TEST_CASE("System - semantic Is cached runs refresh after descendant changes") {
	TestWorld twld;

	const auto root = wld.add();
	const auto child = wld.add();
	wld.as(child, root);
	wld.add<Position>(child, {1.0f, 0.0f, 0.0f});

	uint32_t hits = 0;
	float sum = 0.0f;
	auto sys = wld.system().all<Position>().is(root).on_each([&](const Position& pos) {
		++hits;
		sum += pos.x;
	});

	auto run_system = [&]() {
		hits = 0;
		sum = 0.0f;
		sys.exec();
		return std::pair{hits, sum};
	};

	auto [hitsInitial, sumInitial] = run_system();
	CHECK(hitsInitial == 1);
	CHECK(sumInitial == doctest::Approx(1.0f));

	auto [hitsCached, sumCached] = run_system();
	CHECK(hitsCached == 1);
	CHECK(sumCached == doctest::Approx(1.0f));

	const auto grandchild = wld.add();
	wld.as(grandchild, child);
	wld.add<Position>(grandchild, {2.0f, 0.0f, 0.0f});

	auto [hitsAdded, sumAdded] = run_system();
	CHECK(hitsAdded == 2);
	CHECK(sumAdded == doctest::Approx(3.0f));

	wld.del<Position>(child);

	auto [hitsRemoved, sumRemoved] = run_system();
	CHECK(hitsRemoved == 1);
	CHECK(sumRemoved == doctest::Approx(2.0f));
}
