#include "test_common.h"

#define TestWorld SparseTestWorld

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
									.name("sys1")
									.all<Position>()
									.all<Acceleration>() //
									.on_each([&](Position, Acceleration) {
										if (sys1_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys1 = true;
										++sys1_cnt;
									});
	auto sys2 = wld.system()
									.name("sys2")
									.all<Position>() //
									.on_each([&](ecs::Iter& it) {
										if (sys2_cnt == 0 && sys3_cnt > 0)
											sys3_run_before_sys2 = true;
										GAIA_EACH(it)++ sys2_cnt;
									});
	auto sys3 = wld.system()
									.name("sys3")
									.all<Acceleration>() //
									.on_each([&](ecs::Iter& it) {
										GAIA_EACH(it)++ sys3_cnt;
									});

	testRun();

	// DependsOn is scheduled as depth-first postorder, so dependents run before their target.
	wld.add(sys1.entity(), {ecs::DependsOn, sys3.entity()});
	wld.add(sys2.entity(), {ecs::DependsOn, sys3.entity()});

	testRun();

	CHECK_FALSE(sys3_run_before_sys1);
	CHECK_FALSE(sys3_run_before_sys2);
}

TEST_CASE("World - frame cleanup does not run systems") {
	TestWorld twld;
	uint32_t sysCnt = 0;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});
	wld.system().all<Position>().on_each([&](ecs::Iter& it) {
		sysCnt += it.size();
	});

	wld.del(e);
	wld.frame_cleanup();

	CHECK(sysCnt == 0);
	CHECK_FALSE(wld.has(e));
}

TEST_CASE("System - iterator command buffer is visible to later system") {
	struct SystemDeferredSource {};
	struct SystemDeferredResult {};

	TestWorld twld;
	(void)wld.add<SystemDeferredSource>();
	(void)wld.add<SystemDeferredResult>();

	const auto e = wld.add();
	wld.add<SystemDeferredSource>(e);

	uint32_t producerHits = 0;
	uint32_t consumerHits = 0;

	auto producer = wld.system().all<SystemDeferredSource>().on_each([&](ecs::Iter& it) {
		auto& cb = it.cmd_buffer_st();
		auto ev = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			cb.add<SystemDeferredResult>(ev[i]);
			++producerHits;
		}
	});

	auto consumer = wld.system().all<SystemDeferredResult>().on_each([&](ecs::Iter& it) {
		consumerHits += it.size();
	});

	wld.add(producer.entity(), {ecs::DependsOn, consumer.entity()});
	wld.systems_run();

	CHECK(producerHits == 1);
	CHECK(consumerHits == 1);
	CHECK(wld.has<SystemDeferredResult>(e));
}

TEST_CASE("System - iterator command buffer instantiate is visible to later system") {
	struct CmdBufPrefabTick {};

	TestWorld twld;
	(void)wld.add<CmdBufPrefabTick>();

	const auto prefab = wld.prefab();
	wld.add<Position>(prefab, {4, 5, 6});
	const auto tick = wld.add();
	wld.add<CmdBufPrefabTick>(tick);

	uint32_t producerHits = 0;
	uint32_t consumerHits = 0;

	auto producer = wld.system().all<CmdBufPrefabTick>().on_each([&](ecs::Iter& it) {
		auto& cb = it.cmd_buffer_st();
		GAIA_EACH(it) {
			const auto tmp = cb.instantiate(prefab);
			cb.set<Position>(tmp, {9, 8, 7});
			++producerHits;
		}
	});

	auto consumer = wld.system().all<Position>().is(prefab).on_each([&](ecs::Iter& it) {
		auto pv = it.view<Position>();
		GAIA_EACH(it) {
			CHECK(pv[i].x == 9.0f);
			CHECK(pv[i].y == 8.0f);
			CHECK(pv[i].z == 7.0f);
			++consumerHits;
		}
	});

	wld.add(producer.entity(), {ecs::DependsOn, consumer.entity()});
	wld.systems_run();

	CHECK(producerHits == 1);
	CHECK(consumerHits == 1);
	CHECK(wld.query().is(prefab).count() == 1);
}

TEST_CASE("System - direct Is seeds exclude an instance deleted by an earlier system") {
	struct DeletePrefabInstance {};
	struct InspectPrefabInstances {};

	TestWorld twld;
	const auto prefab = wld.prefab();
	const auto instance = wld.instantiate(prefab);
	const auto inspector = wld.add();
	wld.add<DeletePrefabInstance>(instance);
	wld.add<InspectPrefabInstances>(inspector);

	uint32_t inHits = 0;
	uint32_t isHits = 0;

	auto producer = wld.system().all<DeletePrefabInstance>().on_each([&](ecs::Iter& it) {
		auto& cb = it.cmd_buffer_st();
		auto entities = it.view<ecs::Entity>();
		GAIA_EACH(it)
		cb.del(entities[i]);
	});

	auto consumer = wld.system().all<InspectPrefabInstances>().on_each([&]() {
		wld.query().in(prefab).each([&](ecs::Entity) {
			++inHits;
		});
		wld.query().is(prefab).each([&](ecs::Entity) {
			++isHits;
		});
	});

	wld.add(producer.entity(), {ecs::DependsOn, consumer.entity()});
	wld.systems_run();

	CHECK_FALSE(wld.valid(instance));
	CHECK(inHits == 0);
	CHECK(isHits == 0);
}

TEST_CASE("System - iterator command buffer crosses phase boundary") {
	struct PhaseDeferredSource {};
	struct PhaseDeferredResult {};

	TestWorld twld;
	(void)wld.add<PhaseDeferredSource>();
	(void)wld.add<PhaseDeferredResult>();

	const auto producerPhase = wld.add();
	const auto consumerPhase = wld.add();
	wld.add(producerPhase, {ecs::DependsOn, consumerPhase});

	const auto e = wld.add();
	wld.add<PhaseDeferredSource>(e);

	uint32_t producerHits = 0;
	uint32_t consumerHits = 0;

	wld.system().phase(producerPhase).all<PhaseDeferredSource>().on_each([&](ecs::Iter& it) {
		auto& cb = it.cmd_buffer_st();
		auto ev = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			cb.add<PhaseDeferredResult>(ev[i]);
			++producerHits;
		}
	});

	wld.system().phase(consumerPhase).all<PhaseDeferredResult>().on_each([&](ecs::Iter& it) {
		consumerHits += it.size();
	});

	wld.systems_run();

	CHECK(producerHits == 1);
	CHECK(consumerHits == 1);
	CHECK(wld.has<PhaseDeferredResult>(e));
}

TEST_CASE("System - nested retained query writes are visible to later system") {
	struct NestedTick {};
	struct NestedWork {
		uint32_t requests = 0;
	};

	TestWorld twld;
	(void)wld.add<NestedTick>();
	(void)wld.add<NestedWork>();

	const auto tick = wld.add();
	wld.add<NestedTick>(tick);

	const auto worker = wld.add();
	wld.add<NestedWork>(worker, {});

	uint32_t tickHits = 0;
	uint32_t requestCount = 0;

	auto workQuery = wld.query().all<NestedWork&>();
	struct NestedSystemCtx {
		ecs::Query* workQuery;
		uint32_t nestedHits = 0;
		ecs::Entity requestEntity;
	};
	NestedSystemCtx ctx{&workQuery, 0, {}};

	auto producer = wld.system().ctx(&ctx).all<NestedTick>().writes<NestedWork>().on_each([&](ecs::Iter& it) {
		auto& data = *static_cast<NestedSystemCtx*>(it.ctx());
		tickHits += it.size();

		data.workQuery->each([&](ecs::Iter& workIt) {
			auto entities = workIt.view<ecs::Entity>();
			auto workView = workIt.view_mut<NestedWork>();
			GAIA_EACH(workIt) {
				++workView[i].requests;
				data.requestEntity = entities[i];
				++data.nestedHits;
			}
		});
	});

	auto consumer = wld.system().all<const NestedWork>().on_each([&](ecs::Iter& it) {
		auto workView = it.view<NestedWork>();
		GAIA_EACH(it) {
			requestCount += workView[i].requests;
		}
	});

	wld.add(producer.entity(), {ecs::DependsOn, consumer.entity()});
	wld.systems_run();

	CHECK(tickHits == 1);
	CHECK(ctx.nestedHits == 1);
	CHECK(requestCount == 1);
	CHECK(ctx.requestEntity == worker);
	CHECK(wld.get<NestedWork>(worker).requests == 1);
}

TEST_CASE("System - builder exposes kind and scope") {
	TestWorld twld;

	const auto systemEntity = wld.system()
																.kind(ecs::QueryCacheKind::All)
																.scope(ecs::QueryCacheScope::Shared)
																.all<Position>()
																.on_each([](ecs::Iter&) {})
																.entity();

	auto ss = wld.acc(systemEntity);
	const auto& sys = ss.get<ecs::System_>();
	CHECK(sys.query.kind() == ecs::QueryCacheKind::All);
	CHECK(sys.query.scope() == ecs::QueryCacheScope::Shared);
}

TEST_CASE("System - invalid kind reports reason") {
	TestWorld twld;

	const auto systemEntity =
			wld.system().kind(ecs::QueryCacheKind::All).no<Position>().on_each([](ecs::Iter&) {}).entity();

	auto ss = wld.acc_mut(systemEntity);
	auto& sys = ss.smut<ecs::System_>();
	CHECK_FALSE(sys.query.valid());
	CHECK(sys.query.kind_error() == ecs::QueryKindRes::AllNotIm);
	CHECK(cstr_view(sys.query.kind_error_str()).find("immediate") != BadIndex);
}

TEST_CASE("System - builder context is visible from iterator callbacks") {
	struct SystemCtx {
		uint32_t hits = 0;
		uint32_t total = 0;
	};

	TestWorld twld;
	SystemCtx ctx{};

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});
	(void)wld.copy(e);

	auto sys = wld.system().ctx(&ctx).all<Position>().on_each([](ecs::Iter& it) {
		auto& data = *static_cast<SystemCtx*>(it.ctx());
		++data.hits;
		data.total += it.size();
	});

	CHECK(sys.ctx() == &ctx);

	sys.exec();

	SystemCtx replacement{};
	sys.ctx(&replacement);
	sys.exec();

	CHECK(ctx.hits == 1);
	CHECK(ctx.total == 2);
	CHECK(replacement.hits == 1);
	CHECK(replacement.total == 2);
}

TEST_CASE("Query - context is visible from iterator callbacks") {
	struct QueryCtx {
		uint32_t hits = 0;
		uint32_t total = 0;
	};

	TestWorld twld;
	QueryCtx ctx{};

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});
	(void)wld.copy(e);

	auto q = wld.query().ctx(&ctx).all<Position>();

	CHECK(q.ctx() == &ctx);

	q.each([](ecs::Iter& it) {
		auto* data = static_cast<QueryCtx*>(it.ctx());
		CHECK(data != nullptr);
		++data->hits;
		data->total += it.size();
	});

	QueryCtx replacement{};
	q.ctx(&replacement);
	q.each([](ecs::Iter& it) {
		auto* data = static_cast<QueryCtx*>(it.ctx());
		CHECK(data != nullptr);
		++data->hits;
		data->total += it.size();
	});

	CHECK(ctx.hits == 1);
	CHECK(ctx.total == 2);
	CHECK(replacement.hits == 1);
	CHECK(replacement.total == 2);
}

TEST_CASE("Query - shared cached queries keep separate contexts") {
	struct QueryCtx {
		uint32_t hits = 0;
	};

	TestWorld twld;
	QueryCtx first{};
	QueryCtx second{};

	auto e = wld.add();
	wld.add<Position>(e, {1, 2, 3});

	auto qFirst = wld.query().scope(ecs::QueryCacheScope::Shared).ctx(&first).all<Position>();
	auto qSecond = wld.query().scope(ecs::QueryCacheScope::Shared).ctx(&second).all<Position>();

	qFirst.each([](ecs::Iter& it) {
		auto* data = static_cast<QueryCtx*>(it.ctx());
		CHECK(data != nullptr);
		++data->hits;
	});
	qSecond.each([](ecs::Iter& it) {
		auto* data = static_cast<QueryCtx*>(it.ctx());
		CHECK(data != nullptr);
		++data->hits;
	});

	CHECK(qFirst.id() == qSecond.id());
	CHECK(first.hits == 1);
	CHECK(second.hits == 1);
}

TEST_CASE("Query - access declarations describe scheduling conflicts") {
	TestWorld twld;

	auto readPosA = wld.query().all<const Position>();
	auto readPosB = wld.query().all<const Position>().reads<Acceleration>();
	auto writePos = wld.query().all<Position&>();
	auto writeAccel = wld.query().all<const Position>().writes<Acceleration>();
	auto readScale = wld.query().all<const Position>().reads<Scale>();

	CHECK(readPosB.custom_reads().size() == 1);
	CHECK(writeAccel.custom_writes().size() == 1);
	const auto accelId = readPosB.custom_reads()[0];
	CHECK(readPosB.access(accelId) == ecs::QueryAccess::Read);
	CHECK(writeAccel.access(accelId) == ecs::QueryAccess::Write);

	CHECK(readPosA.can_run_parallel(readPosB));
	CHECK_FALSE(readPosA.can_run_parallel(writePos));
	CHECK_FALSE(writePos.can_run_parallel(readPosA));
	CHECK_FALSE(readPosB.can_run_parallel(writeAccel));
	CHECK(writeAccel.can_run_parallel(readScale));

	auto sharedRead = wld.query().scope(ecs::QueryCacheScope::Shared).all<const Position>().reads<Acceleration>();
	auto sharedWrite = wld.query().scope(ecs::QueryCacheScope::Shared).all<const Position>().writes<Acceleration>();
	CHECK(sharedRead.id() == sharedWrite.id());
	CHECK_FALSE(sharedRead.can_run_parallel(sharedWrite));

	auto mainOnly = wld.query().all<const Position>().main_thread();
	CHECK(mainOnly.main_thread_required());
	CHECK_FALSE(mainOnly.can_run_parallel(readPosA));
}

TEST_CASE("Query - scheduling conflicts are symmetric across effective access sources") {
	TestWorld twld;
	const auto relation = wld.add();
	const auto target = wld.add();
	const auto otherTarget = wld.add();
	const auto resource = ecs::Pair(relation, target);
	const auto otherResource = ecs::Pair(relation, otherTarget);
	const auto check = [](ecs::Query& left, ecs::Query& right, bool expected) {
		CHECK(left.conflicts_with(right) == expected);
		CHECK(right.conflicts_with(left) == expected);
	};

	auto pairRead = wld.query().reads(resource);
	auto pairWrite = wld.query().writes(resource);
	auto otherPairWrite = wld.query().writes(otherResource);
	auto pairMatch = wld.query().all(resource).no_access();
	check(pairRead, pairWrite, true);
	check(pairRead, otherPairWrite, false);
	check(pairMatch, pairWrite, false);

	auto termRead = wld.query().all<const Position>();
	auto termWrite = wld.query().all<Position&>();
	auto customRead = wld.query().reads<Position>();
	auto customWrite = wld.query().writes<Position>();
	check(customRead, termWrite, true);
	check(customWrite, termRead, true);
	check(customRead, termRead, false);
	check(pairRead, customWrite, false);

	auto duplicateAccess =
			wld.query().all<const Position>().reads<Position>().reads<Position>().writes<Position>().writes<Position>();
	CHECK(duplicateAccess.custom_reads().size() == 1);
	CHECK(duplicateAccess.custom_writes().size() == 1);
	check(duplicateAccess, termRead, true);
	check(duplicateAccess, customRead, true);

	auto match = wld.query().all<Position>().no_access();
	auto matchRead = wld.query().all<Position>().no_access().reads<Position>();
	auto matchReadWrite = wld.query().all<Position>().no_access().reads<Position>().writes<Position>();
	check(match, duplicateAccess, false);
	check(matchRead, customWrite, true);
	check(matchRead, termRead, false);
	check(matchReadWrite, termRead, true);

	auto empty = wld.query();
	check(empty, pairWrite, false);
	check(empty, duplicateAccess, false);
	check(empty, matchReadWrite, false);
}

TEST_CASE("Query - exact zero-payload terms default to matching only") {
	struct OtherTag {};
	using TagPair = ecs::pair<Empty, OtherTag>;
	TestWorld twld;
	const auto tag = wld.add<Empty>().entity;
	const auto otherTag = wld.add<OtherTag>().entity;
	const auto position = wld.add<Position>().entity;
	const auto bare = wld.add();
	const auto target = wld.add();
	const auto tagPair = ecs::Pair(tag, otherTag);
	const auto barePair = ecs::Pair(bare, target);
	const auto present = wld.add();
	wld.add<Position>(present, {1, 0, 0});
	wld.add<Empty>(present);
	wld.add(present, tagPair);
	wld.add(present, bare);
	wld.add(present, barePair);
	const auto absent = wld.add();
	wld.add<Position>(absent, {2, 0, 0});

	auto typed = wld.query().scope(ecs::QueryCacheScope::Shared).all<Empty&>();
	auto options = wld.query().scope(ecs::QueryCacheScope::Shared).all<Empty>(ecs::QueryTermOptions{}.write());
	auto explicitMatch = wld.query().scope(ecs::QueryCacheScope::Shared).all<Empty>().no_access();
	auto raw = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::Write, bare});
	auto parsed = wld.query().add("%e", tag.value());
	auto typedPair = wld.query().all<TagPair&>();
	auto parsedPair = wld.query().add("(%e, %e)", bare.value(), target.value());
	auto orTerms = wld.query().all<Position>().or_<Empty&>().or_(barePair, ecs::QueryTermOptions{}.write());
	auto optional = wld.query().all<Position>().any<Empty&>().any(tagPair, ecs::QueryTermOptions{}.write());
	for (auto* query: {&typed, &options, &explicitMatch, &raw, &parsed, &typedPair, &parsedPair, &orTerms})
		CHECK(query->count() == 1);
	CHECK(optional.count() == 2);
	CHECK(typed.id() == options.id());
	CHECK(typed.id() == explicitMatch.id());
	CHECK(typed.access(tag) == ecs::QueryAccess::None);
	CHECK(raw.access(bare) == ecs::QueryAccess::None);
	CHECK(parsed.access(tag) == ecs::QueryAccess::None);
	CHECK(typedPair.access(tagPair) == ecs::QueryAccess::None);
	CHECK(parsedPair.access(barePair) == ecs::QueryAccess::None);
	CHECK(orTerms.access(tag) == ecs::QueryAccess::None);
	CHECK(orTerms.access(barePair) == ecs::QueryAccess::None);
	CHECK(optional.access(tag) == ecs::QueryAccess::None);
	CHECK(optional.access(tagPair) == ecs::QueryAccess::None);
	CHECK(orTerms.access(position) == ecs::QueryAccess::Read);
	auto payloadWrite = wld.query().all<Position&>();
	CHECK(payloadWrite.access(position) == ecs::QueryAccess::Write);

	auto customWriter = wld.query().writes<Empty>();
	CHECK(customWriter.access(tag) == ecs::QueryAccess::Write);
	CHECK_FALSE(typed.conflicts_with(customWriter));
	CHECK_FALSE(customWriter.conflicts_with(typed));
	typed.reads<Empty>();
	CHECK(typed.access(tag) == ecs::QueryAccess::Read);
	CHECK(typed.conflicts_with(customWriter));
	CHECK(customWriter.conflicts_with(typed));
}

TEST_CASE("Query - data pair access follows payload and pair overlap") {
	using RelationPair = ecs::pair<Position, Something>;
	using TargetPair = ecs::pair<Empty, Position>;
	TestWorld twld;
	const auto position = wld.add<Position>().entity;
	const auto something = wld.add<Something>().entity;
	const auto tag = wld.add<Empty>().entity;
	const auto target = wld.add();
	const auto relationPair = ecs::Pair(position, something);
	const auto targetPair = ecs::Pair(tag, position);
	const auto otherPair = ecs::Pair(position, target);
	const auto present = wld.add();
	wld.add<RelationPair>(present, {1, 2, 3});
	wld.add<TargetPair>(present, {4, 5, 6});
	CHECK(wld.comp_cache().find_pair_payload(relationPair)->entity == position);
	CHECK(wld.comp_cache().find_pair_payload(targetPair)->entity == position);
	const auto check = [](ecs::Query& left, ecs::Query& right, bool expected) {
		CHECK(left.conflicts_with(right) == expected);
		CHECK(right.conflicts_with(left) == expected);
	};

	auto read = wld.query().all<RelationPair>();
	auto write = wld.query().all<RelationPair&>();
	auto targetWrite = wld.query().any<TargetPair&>();
	auto otherWrite = wld.query().all(otherPair, ecs::QueryTermOptions{}.write());
	auto rawWrite = wld.query().add({ecs::QueryOpKind::All, ecs::QueryAccess::Write, targetPair});
	auto parsed = wld.query().add("(%e, %e)", position.value(), something.value());
	auto match = wld.query().all<RelationPair&>().no_access();
	CHECK(read.count() == 1);
	CHECK(write.count() == 1);
	CHECK(read.access(relationPair) == ecs::QueryAccess::Read);
	CHECK(write.access(relationPair) == ecs::QueryAccess::Write);
	CHECK(targetWrite.access(targetPair) == ecs::QueryAccess::Write);
	CHECK(rawWrite.access(targetPair) == ecs::QueryAccess::Write);
	CHECK(parsed.access(relationPair) == ecs::QueryAccess::Read);
	CHECK(match.access(relationPair) == ecs::QueryAccess::None);
	check(read, parsed, false);
	check(read, write, true);
	check(write, otherWrite, false);
	check(write, targetWrite, false);
	check(match, write, false);

	const auto relationWildcard = ecs::Pair(position, ecs::All);
	const auto targetWildcard = ecs::Pair(ecs::All, something);
	const auto tagWildcard = ecs::Pair(tag, ecs::All);
	const auto variablePair = ecs::Pair(position, ecs::Var0);
	auto wildcardRead = wld.query().all(relationWildcard);
	auto wildcardWrite = wld.query().all(targetWildcard, ecs::QueryTermOptions{}.write());
	auto unknownPayload = wld.query().all(tagWildcard);
	auto variableRead = wld.query().all(variablePair);
	CHECK(wildcardRead.access(relationWildcard) == ecs::QueryAccess::Read);
	CHECK(wildcardWrite.access(targetWildcard) == ecs::QueryAccess::Write);
	CHECK(unknownPayload.access(tagWildcard) == ecs::QueryAccess::Read);
	CHECK(variableRead.access(variablePair) == ecs::QueryAccess::Read);
	check(wildcardRead, write, true);
	check(wildcardWrite, read, true);
	check(wildcardRead, wildcardWrite, true);
	check(wildcardWrite, otherWrite, false);
	check(variableRead, write, true);
	check(unknownPayload, targetWrite, true);

	auto customExactRead = wld.query().reads(relationPair);
	auto customExactWrite = wld.query().writes(relationPair);
	auto customOtherWrite = wld.query().writes(targetPair);
	auto customWildcardRead = wld.query().reads(relationWildcard);
	auto customVariableRead = wld.query().reads(variablePair);
	auto customWildcardWrite = wld.query().writes(targetWildcard);
	for (auto* query: {&customWildcardRead, &customVariableRead}) {
		check(*query, customExactWrite, true);
		check(*query, write, true);
		check(*query, customOtherWrite, false);
		check(*query, targetWrite, false);
	}
	check(customWildcardWrite, customExactRead, true);
	check(customWildcardWrite, read, true);
	check(customWildcardWrite, otherWrite, false);
}

TEST_CASE("System - access declarations are stored on the underlying query") {
	TestWorld twld;

	auto sys =
			wld.system().all<const Position>().main_thread().reads<Acceleration>().writes<Scale>().on_each([](ecs::Iter&) {});
	auto ss = wld.acc_mut(sys.entity());
	auto& data = ss.smut<ecs::System_>();

	CHECK(data.query.custom_reads().size() == 1);
	CHECK(data.query.custom_writes().size() == 1);
	CHECK(data.query.main_thread_required());
	CHECK(data.query.access(data.query.custom_reads()[0]) == ecs::QueryAccess::Read);
	CHECK(data.query.access(data.query.custom_writes()[0]) == ecs::QueryAccess::Write);
}

TEST_CASE("Query - presence-only terms preserve matching and sorted payload access") {
	TestWorld twld;
	const auto position = wld.add<Position>().entity;
	const auto acceleration = wld.add<Acceleration>().entity;
	const auto matched = wld.add();
	wld.add<Position>(matched, {1, 0, 0});
	wld.add<Acceleration>(matched, {10, 0, 0});
	const auto missing = wld.add();
	wld.add<Position>(missing, {2, 0, 0});

	for (const bool cached: {false, true}) {
		auto q = (cached ? wld.query() : wld.uquery()).all<Acceleration&>().no_access().all<Position&>();
		CHECK(q.count() == 1);
		CHECK(q.access(acceleration) == ecs::QueryAccess::None);
		CHECK(q.access(position) == ecs::QueryAccess::Write);
		q.each([&](ecs::Iter& it) {
			auto entities = it.view<ecs::Entity>();
			auto pos = it.view_mut<Position>(1);
			GAIA_EACH(it) {
				CHECK(entities[i] == matched);
				pos[i].x += 1;
			}
		});
		q.each([](Position& pos) {
			pos.x += 1;
		});
	}
	CHECK(wld.get<Position>(matched).x == 5);
	CHECK(wld.get<Position>(missing).x == 2);
	CHECK(wld.get<Acceleration>(matched).x == 10);
}

TEST_CASE("Query - no_access changes only the preceding term") {
	TestWorld twld;
	const auto position = wld.add<Position>().entity;
	const auto acceleration = wld.add<Acceleration>().entity;
	const auto scale = wld.add<Scale>().entity;
	const auto entity = wld.add();
	wld.add<Position>(entity, {1, 0, 0});
	wld.add<Acceleration>(entity, {2, 0, 0});
	wld.add<Scale>(entity, {3, 0, 0});

	auto q = wld.query().all<Position&>().all<const Acceleration>().no_access().all<Scale>();
	CHECK(q.count() == 1);
	CHECK(q.access(position) == ecs::QueryAccess::Write);
	CHECK(q.access(acceleration) == ecs::QueryAccess::None);
	CHECK(q.access(scale) == ecs::QueryAccess::Read);
	q.each([](Position& pos, const Scale& scl) {
		pos.x += scl.x;
	});
	CHECK(wld.get<Position>(entity).x == 4);
	CHECK(wld.get<Acceleration>(entity).x == 2);
}

TEST_CASE("Query - no_access preserves interleaved metadata and later terms") {
	TestWorld twld;
	const auto position = wld.add<Position>().entity;
	const auto acceleration = wld.add<Acceleration>().entity;
	const auto scale = wld.add<Scale>().entity;
	auto q = wld.query()
							 .all<Position&>()
							 .all<Acceleration>()
							 .changed<Position>()
							 .match_prefab()
							 .group_dep(ecs::ChildOf)
							 .group_by(ecs::ChildOf)
							 .sort_by<Position>([](const ecs::World&, const void* lhs, const void* rhs) {
								 const auto x = static_cast<const Position*>(lhs)->x;
								 const auto y = static_cast<const Position*>(rhs)->x;
								 return (x > y) - (x < y);
							 })
							 .no_access()
							 .all<Scale>();
	CHECK(q.access(position) == ecs::QueryAccess::Write);
	CHECK(q.access(acceleration) == ecs::QueryAccess::None);
	CHECK(q.access(scale) == ecs::QueryAccess::Read);
	const auto& data = q.fetch().ctx().data;
	CHECK(data.sortBy == position);
	CHECK(data.groupBy == ecs::ChildOf);
	CHECK(core::has(data.changed_view(), position));
	CHECK(core::has(data.group_deps_view(), ecs::ChildOf));
	CHECK((data.flags & ecs::QueryCtx::QueryFlags::MatchPrefab) != 0);
}

TEST_CASE("Query - presence-only options and shared cache preserve term access") {
	TestWorld twld;
	const auto position = wld.add<Position>().entity;
	const auto acceleration = wld.add<Acceleration>().entity;
	const auto entity = wld.add();
	wld.add<Position>(entity, {1, 0, 0});
	wld.add<Acceleration>(entity, {2, 0, 0});

	auto qMatch = wld.query().scope(ecs::QueryCacheScope::Shared).all<Acceleration>().no_access().all<Position>();
	auto qTyped = wld.query()
										.scope(ecs::QueryCacheScope::Shared)
										.all<Acceleration>(ecs::QueryTermOptions{}.no_access())
										.all<Position>();
	auto qEntity = wld.query()
										 .scope(ecs::QueryCacheScope::Shared)
										 .all(acceleration, ecs::QueryTermOptions{}.no_access())
										 .all(position);
	auto qNoAccess =
			wld.query().scope(ecs::QueryCacheScope::Shared).all<Acceleration>().no_access().no_access().all<Position>();
	auto qReversed = wld.query().scope(ecs::QueryCacheScope::Shared).all<Position>().all<Acceleration>().no_access();
	auto qRead = wld.query().scope(ecs::QueryCacheScope::Shared).all<Acceleration>().all<Position>();

	CHECK(qMatch.count() == 1);
	CHECK(qTyped.count() == 1);
	CHECK(qEntity.count() == 1);
	CHECK(qNoAccess.count() == 1);
	CHECK(qReversed.count() == 1);
	CHECK(qRead.count() == 1);
	CHECK(qMatch.id() == qTyped.id());
	CHECK(qMatch.id() == qEntity.id());
	CHECK(qMatch.id() == qNoAccess.id());
	CHECK(qMatch.id() != qRead.id());
	CHECK(qMatch.id() != qReversed.id());
	CHECK(qMatch.access(acceleration) == ecs::QueryAccess::None);
	CHECK(qTyped.access(acceleration) == ecs::QueryAccess::None);
	CHECK(qEntity.access(acceleration) == ecs::QueryAccess::None);
	CHECK(qRead.access(acceleration) == ecs::QueryAccess::Read);

	qMatch.each([](ecs::Iter& it) {
		auto pos = it.view<Position>(1);
		GAIA_EACH(it) CHECK(pos[i].x == 1);
	});
	qReversed.each([](ecs::Iter& it) {
		auto pos = it.view<Position>(0);
		GAIA_EACH(it) CHECK(pos[i].x == 1);
	});

	auto qWrite = wld.query().scope(ecs::QueryCacheScope::Shared).all<Acceleration>().no_access().all<Position&>();
	CHECK(qWrite.count() == 1);
	auto qWriteAgain = wld.query().scope(ecs::QueryCacheScope::Shared).all<Acceleration&>().no_access().all<Position&>();
	CHECK(qWriteAgain.count() == 1);
	CHECK(qWrite.id() == qWriteAgain.id());
	CHECK(qWriteAgain.access(acceleration) == ecs::QueryAccess::None);
	CHECK(qWriteAgain.access(position) == ecs::QueryAccess::Write);
	CHECK_FALSE(qWriteAgain.can_run_parallel(qRead));
}

TEST_CASE("Query - presence-only or terms retain matching and explicit scheduling declarations") {
	TestWorld twld;
	const auto acceleration = wld.add<Acceleration>().entity;
	const auto scale = wld.add<Scale>().entity;
	const auto accelerated = wld.add();
	wld.add<Position>(accelerated, {1, 0, 0});
	wld.add<Acceleration>(accelerated, {2, 0, 0});
	const auto scaled = wld.add();
	wld.add<Position>(scaled, {3, 0, 0});
	wld.add<Scale>(scaled, {4, 0, 0});
	const auto missing = wld.add();
	wld.add<Position>(missing, {5, 0, 0});

	auto q = wld.query().all<Position>().or_<Acceleration&>().no_access().or_<Scale>().no_access();
	auto writer = wld.query().all<Acceleration&>().all<Scale&>();
	CHECK(q.count() == 2);
	CHECK(q.access(acceleration) == ecs::QueryAccess::None);
	CHECK(q.access(scale) == ecs::QueryAccess::None);
	CHECK(q.can_run_parallel(writer));
	CHECK(writer.can_run_parallel(q));

	auto qMixed = wld.query().all<Position>().or_<Acceleration>().no_access().or_<Scale>();
	auto accelerationWriter = wld.query().all<Acceleration&>();
	auto scaleWriter = wld.query().all<Scale&>();
	CHECK(qMixed.count() == 2);
	CHECK(qMixed.access(acceleration) == ecs::QueryAccess::None);
	CHECK(qMixed.access(scale) == ecs::QueryAccess::Read);
	CHECK(qMixed.can_run_parallel(accelerationWriter));
	CHECK_FALSE(qMixed.can_run_parallel(scaleWriter));

	q.reads<Acceleration>();
	CHECK(q.access(acceleration) == ecs::QueryAccess::Read);
	CHECK_FALSE(q.can_run_parallel(writer));
	CHECK_FALSE(writer.can_run_parallel(q));
	q.writes<Scale>();
	CHECK(q.access(scale) == ecs::QueryAccess::Write);
	CHECK(q.count() == 2);

	wld.del<Acceleration>(accelerated);
	CHECK(q.count() == 1);
	q.each([&](ecs::Entity entity) {
		CHECK(entity == scaled);
	});
}

TEST_CASE("Query - optional no_access terms preserve rows and scheduling metadata") {
	TestWorld twld;
	const auto position = wld.add<Position>().entity;
	const auto scale = wld.add<Scale>().entity;
	const auto present = wld.add();
	wld.add<Position>(present, {1, 0, 0});
	wld.add<Scale>(present, {2, 0, 0});
	const auto absent = wld.add();
	wld.add<Scale>(absent, {3, 0, 0});

	auto q = wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale>().any<Position>().no_access();
	auto options =
			wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale>().any<Position>(ecs::QueryTermOptions{}.no_access());
	auto entity =
			wld.query().scope(ecs::QueryCacheScope::Shared).all(scale).any(position, ecs::QueryTermOptions{}.no_access());
	auto input = wld.query()
									 .scope(ecs::QueryCacheScope::Shared)
									 .all(scale)
									 .add({ecs::QueryOpKind::Any, ecs::QueryAccess::Match, position});
	auto ordinary = wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale>().any<Position>();
	auto uncached = wld.uquery().all(scale).any(position).no_access();
	CHECK(q.count() == 2);
	CHECK(options.count() == 2);
	CHECK(entity.count() == 2);
	CHECK(input.count() == 2);
	CHECK(ordinary.count() == 2);
	CHECK(uncached.count() == 2);
	CHECK(q.id() == options.id());
	CHECK(q.id() == entity.id());
	CHECK(q.id() == input.id());
	CHECK(q.id() != ordinary.id());
	CHECK(q.access(position) == ecs::QueryAccess::None);
	CHECK(q.access(scale) == ecs::QueryAccess::Read);
	CHECK(ordinary.access(position) == ecs::QueryAccess::Read);
	CHECK(uncached.access(position) == ecs::QueryAccess::None);

	uint32_t withPosition = 0;
	uint32_t withoutPosition = 0;
	q.each([&](ecs::Iter& it) {
		if (it.has<Position>())
			withPosition += it.size();
		else
			withoutPosition += it.size();
	});
	CHECK(withPosition == 1);
	CHECK(withoutPosition == 1);

	auto writer = wld.query().all<Position&>();
	CHECK_FALSE(ordinary.can_run_parallel(writer));
	CHECK_FALSE(writer.can_run_parallel(ordinary));
	CHECK(q.can_run_parallel(writer));
	CHECK(writer.can_run_parallel(q));
	q.reads<Position>();
	CHECK(q.access(position) == ecs::QueryAccess::Read);
	CHECK_FALSE(q.can_run_parallel(writer));
	CHECK_FALSE(writer.can_run_parallel(q));
	q.writes<Position>();
	CHECK(q.access(position) == ecs::QueryAccess::Write);
	auto reader = wld.query().all<const Position>();
	CHECK_FALSE(q.can_run_parallel(reader));
	CHECK_FALSE(reader.can_run_parallel(q));

	uint32_t systemRows = 0;
	auto system = wld.system().all<Scale>().any<Position>().no_access().on_each([&](const Scale&) {
		++systemRows;
	});
	system.exec();
	CHECK(systemRows == 2);
	wld.del<Position>(present);
	CHECK(q.count() == 2);
}

TEST_CASE("Query - optional payload terms infer read and write scheduling access") {
	TestWorld twld;
	const auto position = wld.add<Position>().entity;
	const auto scale = wld.add<Scale>().entity;
	const auto present = wld.add();
	wld.add<Position>(present, {1, 0, 0});
	wld.add<Scale>(present, {2, 0, 0});
	const auto absent = wld.add();
	wld.add<Scale>(absent, {3, 0, 0});

	auto read = wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale>().any<Position>();
	auto readConst = wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale>().any<const Position>();
	auto write = wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale>().any<Position&>();
	auto readOptions =
			wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale>().any<Position&>(ecs::QueryTermOptions{}.read());
	auto writeOptions =
			wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale>().any<const Position>(ecs::QueryTermOptions{}.write());
	auto readEntity = wld.query().scope(ecs::QueryCacheScope::Shared).all(scale).any(position);
	auto readInput = wld.query()
											 .scope(ecs::QueryCacheScope::Shared)
											 .all(scale)
											 .add({ecs::QueryOpKind::Any, ecs::QueryAccess::Read, position});
	auto writeEntity =
			wld.query().scope(ecs::QueryCacheScope::Shared).all(scale).any(position, ecs::QueryTermOptions{}.write());
	auto writeInput = wld.query()
												.scope(ecs::QueryCacheScope::Shared)
												.all(scale)
												.add({ecs::QueryOpKind::Any, ecs::QueryAccess::Write, position});
	for (auto* query: {&read, &readConst, &readOptions, &readEntity, &readInput}) {
		CHECK(query->count() == 2);
		CHECK(query->access(position) == ecs::QueryAccess::Read);
		CHECK(query->id() == read.id());
		CHECK_FALSE(query->conflicts_with(read));
		CHECK_FALSE(read.conflicts_with(*query));
	}
	for (auto* query: {&write, &writeOptions, &writeEntity, &writeInput}) {
		CHECK(query->count() == 2);
		CHECK(query->access(position) == ecs::QueryAccess::Write);
		CHECK(query->id() == write.id());
		CHECK(query->id() != read.id());
		CHECK(query->conflicts_with(read));
		CHECK(read.conflicts_with(*query));
		CHECK(query->conflicts_with(write));
		CHECK(write.conflicts_with(*query));
	}

	float readValue = 0;
	uint32_t missingRows = 0;
	read.each([&](ecs::Iter& it) {
		if (!it.has<Position>()) {
			missingRows += it.size();
			return;
		}
		auto values = it.view<Position>(1);
		GAIA_EACH(it) readValue += values[i].x;
	});
	CHECK(readValue == 1);
	CHECK(missingRows == 1);
	write.each([](ecs::Iter& it) {
		if (!it.has<Position>())
			return;
		auto values = it.view_mut<Position>(1);
		GAIA_EACH(it) values[i].x += 4;
	});
	CHECK(wld.get<Position>(present).x == 5);
	CHECK_FALSE(wld.has<Position>(absent));

	const auto relation = wld.add();
	const auto target = wld.add();
	const auto resource = ecs::Pair(relation, target);
	auto pairTerm = wld.query().all<Scale>().any(resource, ecs::QueryTermOptions{}.write());
	auto pairWriter = wld.query().writes(resource);
	CHECK(pairTerm.access(resource) == ecs::QueryAccess::None);
	CHECK_FALSE(pairTerm.conflicts_with(pairWriter));
	CHECK_FALSE(pairWriter.conflicts_with(pairTerm));
}

TEST_CASE("Query - optional shared cache preserves access after term reordering") {
	for (const bool optionalFirst: {false, true}) {
		TestWorld twld;
		const auto position = wld.add<Position>().entity;
		const auto scale = wld.add<Scale>().entity;
		const auto entity = wld.add();
		wld.add<Position>(entity, {1, 0, 0});
		wld.add<Scale>(entity, {2, 0, 0});
		auto requiredWrite = wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale&>().any<Position>();
		auto optionalWrite = wld.query().scope(ecs::QueryCacheScope::Shared).any<Position&>().all<Scale>();
		if (optionalFirst) {
			CHECK(optionalWrite.count() == 1);
			CHECK(requiredWrite.count() == 1);
		} else {
			CHECK(requiredWrite.count() == 1);
			CHECK(optionalWrite.count() == 1);
		}
		CHECK(requiredWrite.id() != optionalWrite.id());
		CHECK(requiredWrite.access(scale) == ecs::QueryAccess::Write);
		CHECK(requiredWrite.access(position) == ecs::QueryAccess::Read);
		CHECK(optionalWrite.access(scale) == ecs::QueryAccess::Read);
		CHECK(optionalWrite.access(position) == ecs::QueryAccess::Write);
		optionalWrite.each([](ecs::Iter& it) {
			auto values = it.view_mut<Position>(0);
			GAIA_EACH(it) values[i].x += 3;
		});
		requiredWrite.each([](ecs::Iter& it) {
			auto values = it.view_mut<Scale>(0);
			GAIA_EACH(it) values[i].x += 4;
		});
		CHECK(wld.get<Position>(entity).x == 4);
		CHECK(wld.get<Scale>(entity).x == 6);
	}
}

TEST_CASE("Query - pair shared cache preserves access after term reordering") {
	using PairPayload = ecs::pair<Position, Empty>;
	for (const bool pairFirst: {false, true}) {
		TestWorld twld;
		const auto position = wld.add<Position>().entity;
		const auto tag = wld.add<Empty>().entity;
		const auto scale = wld.add<Scale>().entity;
		const auto pair = ecs::Pair(position, tag);
		const auto entity = wld.add();
		wld.add<PairPayload>(entity, {1, 0, 0});
		wld.add<Scale>(entity, {2, 0, 0});
		auto scaleWrite = wld.query().scope(ecs::QueryCacheScope::Shared).all<Scale&>().all<PairPayload>();
		auto pairWrite = wld.query().scope(ecs::QueryCacheScope::Shared).all<PairPayload&>().all<Scale>();
		if (pairFirst) {
			CHECK(pairWrite.count() == 1);
			CHECK(scaleWrite.count() == 1);
		} else {
			CHECK(scaleWrite.count() == 1);
			CHECK(pairWrite.count() == 1);
		}
		CHECK(scaleWrite.id() != pairWrite.id());
		CHECK(scaleWrite.access(scale) == ecs::QueryAccess::Write);
		CHECK(scaleWrite.access(pair) == ecs::QueryAccess::Read);
		CHECK(pairWrite.access(scale) == ecs::QueryAccess::Read);
		CHECK(pairWrite.access(pair) == ecs::QueryAccess::Write);
		pairWrite.each([](ecs::Iter& it) {
			auto values = it.view_mut<PairPayload>(0);
			GAIA_EACH(it) values[i].x += 3;
		});
		scaleWrite.each([](ecs::Iter& it) {
			auto values = it.view_mut<Scale>(0);
			GAIA_EACH(it) values[i].x += 4;
		});
		CHECK(wld.get<PairPayload>(entity).x == 4);
		CHECK(wld.get<Scale>(entity).x == 6);
	}
}

TEST_CASE("System - presence-only terms remove payload scheduling conflicts") {
	TestWorld twld;
	const auto position = wld.add<Position>().entity;
	const auto acceleration = wld.add<Acceleration>().entity;
	const auto entity = wld.add();
	wld.add<Position>(entity, {1, 0, 0});
	wld.add<Acceleration>(entity, {2, 0, 0});

	auto filtered = wld.system().all<Position&>().all<const Acceleration>().no_access().on_each([](Position& pos) {
		pos.x += 1;
	});
	auto writer = wld.system().all<Acceleration&>().on_each([](Acceleration& acc) {
		acc.x += 1;
	});
	auto& matchQuery = wld.acc_mut(filtered.entity()).smut<ecs::System_>().query;
	auto& writerQuery = wld.acc_mut(writer.entity()).smut<ecs::System_>().query;
	CHECK(matchQuery.access(acceleration) == ecs::QueryAccess::None);
	CHECK(matchQuery.access(position) == ecs::QueryAccess::Write);
	CHECK(matchQuery.can_run_parallel(writerQuery));
	CHECK(writerQuery.can_run_parallel(matchQuery));

	filtered.exec();
	writer.exec();
	CHECK(wld.get<Position>(entity).x == 2);
	CHECK(wld.get<Acceleration>(entity).x == 3);
}

TEST_CASE("System - dependency depth-first postorder") {
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
	// Systems execute depth-first, deepest child before its DependsOn target: C, A, B, R.
	wld.add(sysA.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysB.entity(), {ecs::DependsOn, sysRoot.entity()});
	wld.add(sysC.entity(), {ecs::DependsOn, sysA.entity()});

	wld.update();

	CHECK(order.size() == 4);
	if (order.size() == 4) {
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'B');
		CHECK(order[3] == 'R');
	}
}

TEST_CASE("System - dependency forest runs depth-first by subtree") {
	cnt::darr<uint32_t> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto make_sys = [&](uint32_t id) {
		return wld.system().all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
	};

	enum : uint32_t { A, B, C, A0, B0, B1, C0, A0x, A0y, B0x, B1x, B1y, C0x, C0y, C0z };

	auto sysA = make_sys(A);
	auto sysB = make_sys(B);
	auto sysC = make_sys(C);
	auto sysA0 = make_sys(A0);
	auto sysB0 = make_sys(B0);
	auto sysB1 = make_sys(B1);
	auto sysC0 = make_sys(C0);
	auto sysA0x = make_sys(A0x);
	auto sysA0y = make_sys(A0y);
	auto sysB0x = make_sys(B0x);
	auto sysB1x = make_sys(B1x);
	auto sysB1y = make_sys(B1y);
	auto sysC0x = make_sys(C0x);
	auto sysC0y = make_sys(C0y);
	auto sysC0z = make_sys(C0z);

	wld.add(sysA0.entity(), {ecs::DependsOn, sysA.entity()});
	wld.add(sysB0.entity(), {ecs::DependsOn, sysB.entity()});
	wld.add(sysB1.entity(), {ecs::DependsOn, sysB.entity()});
	wld.add(sysC0.entity(), {ecs::DependsOn, sysC.entity()});
	wld.add(sysA0x.entity(), {ecs::DependsOn, sysA0.entity()});
	wld.add(sysA0y.entity(), {ecs::DependsOn, sysA0.entity()});
	wld.add(sysB0x.entity(), {ecs::DependsOn, sysB0.entity()});
	wld.add(sysB1x.entity(), {ecs::DependsOn, sysB1.entity()});
	wld.add(sysB1y.entity(), {ecs::DependsOn, sysB1.entity()});
	wld.add(sysC0x.entity(), {ecs::DependsOn, sysC0.entity()});
	wld.add(sysC0y.entity(), {ecs::DependsOn, sysC0.entity()});
	wld.add(sysC0z.entity(), {ecs::DependsOn, sysC0.entity()});

	wld.update();

	const uint32_t expected[] = {A0x, A0y, A0, A, B0x, B0, B1x, B1y, B1, B, C0x, C0y, C0z, C0, C};
	CHECK(order.size() == 15);
	if (order.size() == 15) {
		for (uint32_t i = 0; i < order.size(); ++i)
			CHECK(order[i] == expected[i]);
	}
}

TEST_CASE("System - DependsOn respects all direct dependency targets") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto make_sys = [&](char id) {
		return wld.system().all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
	};

	auto sysA = make_sys('A');
	auto sysB = make_sys('B');
	auto sysB0 = make_sys('b');
	auto sysD = make_sys('D');

	wld.add(sysB0.entity(), {ecs::DependsOn, sysB.entity()});
	wld.add(sysD.entity(), {ecs::DependsOn, sysA.entity()});
	wld.add(sysD.entity(), {ecs::DependsOn, sysB0.entity()});

	wld.update();

	int posA = -1;
	int posB0 = -1;
	int posD = -1;
	for (uint32_t i = 0; i < order.size(); ++i) {
		if (order[i] == 'A')
			posA = (int)i;
		if (order[i] == 'b')
			posB0 = (int)i;
		if (order[i] == 'D')
			posD = (int)i;
	}

	CHECK(order.size() == 4);
	CHECK(posD >= 0);
	CHECK(posA >= 0);
	CHECK(posB0 >= 0);
	CHECK(posD < posA);
	CHECK(posD < posB0);
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
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'B');
		CHECK(order[2] == 'A');
		CHECK(order[3] == 'R');
	}
}

TEST_CASE("System - DependsOn cycle uses deterministic fallback") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto sysA = wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('A');
	});
	auto sysB = wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('B');
	});

	wld.add(sysA.entity(), {ecs::DependsOn, sysB.entity()});
	wld.add(sysB.entity(), {ecs::DependsOn, sysA.entity()});

	wld.update();

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'A');
		CHECK(order[1] == 'B');
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
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'B');
		CHECK(order[2] == 'A');
		CHECK(order[3] == 'R');
	}

	wld.del(sysC.entity(), {ecs::DependsOn, sysB.entity()});
	order.clear();
	wld.update();

	CHECK(order.size() == 4);
	if (order.size() == 4) {
		CHECK(order[0] == 'B');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'C');
		CHECK(order[3] == 'R');
	}
}

TEST_CASE("System - phased systems respect intra-phase DependsOn postorder") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});
	const auto phase = wld.add();

	auto sysA = wld.system().phase(phase).all<Position>().on_each([&order](Position) {
		order.push_back('A');
	});
	auto sysB = wld.system().phase(phase).all<Position>().on_each([&order](Position) {
		order.push_back('B');
	});

	wld.add(sysB.entity(), {ecs::DependsOn, sysA.entity()});
	wld.update();

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'B');
		CHECK(order[1] == 'A');
	}
}

TEST_CASE("System - phase dependencies run deepest phases before their targets") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phaseRoot = wld.add();
	const auto phaseA = wld.add();
	const auto phaseB = wld.add();
	const auto phaseA0 = wld.add();

	auto make_sys = [&](ecs::Entity phase, char id) {
		return wld.system().phase(phase).all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
	};

	make_sys(phaseRoot, 'R');
	make_sys(phaseA, 'A');
	make_sys(phaseB, 'B');
	make_sys(phaseA0, 'a');

	wld.add(phaseA, {ecs::DependsOn, phaseRoot});
	wld.add(phaseB, {ecs::DependsOn, phaseRoot});
	wld.add(phaseA0, {ecs::DependsOn, phaseA});

	wld.update();

	CHECK(order.size() == 4);
	if (order.size() == 4) {
		CHECK(order[0] == 'a');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'B');
		CHECK(order[3] == 'R');
	}
}

TEST_CASE("System - phase dependency rewiring updates schedule order") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phaseRoot = wld.add();
	const auto phaseChild = wld.add();

	wld.system().phase(phaseRoot).all<Position>().on_each([&order](Position) {
		order.push_back('R');
	});
	wld.system().phase(phaseChild).all<Position>().on_each([&order](Position) {
		order.push_back('C');
	});

	wld.add(phaseChild, {ecs::DependsOn, phaseRoot});
	wld.update();

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'R');
	}

	wld.del(phaseChild, {ecs::DependsOn, phaseRoot});
	order.clear();
	wld.update();

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'R');
		CHECK(order[1] == 'C');
	}
}

TEST_CASE("System - disabled phase skips phased systems") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phase = wld.add();
	wld.system().phase(phase).all<Position>().on_each([&order](Position) {
		order.push_back('P');
	});
	wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('U');
	});

	wld.update();
	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'P');
		CHECK(order[1] == 'U');
	}

	wld.enable(phase, false);
	order.clear();
	wld.update();
	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 'U');

	wld.enable(phase, true);
	order.clear();
	wld.update();
	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'P');
		CHECK(order[1] == 'U');
	}
}

TEST_CASE("System - phase dependencies respect all direct dependency targets") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phaseA = wld.add();
	const auto phaseB = wld.add();
	const auto phaseB0 = wld.add();
	const auto phaseD = wld.add();

	auto make_sys = [&](ecs::Entity phase, char id) {
		return wld.system().phase(phase).all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
	};

	make_sys(phaseA, 'A');
	make_sys(phaseB, 'B');
	make_sys(phaseB0, 'b');
	make_sys(phaseD, 'D');

	wld.add(phaseB0, {ecs::DependsOn, phaseB});
	wld.add(phaseD, {ecs::DependsOn, phaseA});
	wld.add(phaseD, {ecs::DependsOn, phaseB0});

	wld.update();

	int posA = -1;
	int posB0 = -1;
	int posD = -1;
	for (uint32_t i = 0; i < order.size(); ++i) {
		if (order[i] == 'A')
			posA = (int)i;
		if (order[i] == 'b')
			posB0 = (int)i;
		if (order[i] == 'D')
			posD = (int)i;
	}

	CHECK(order.size() == 4);
	CHECK(posD >= 0);
	CHECK(posA >= 0);
	CHECK(posB0 >= 0);
	CHECK(posD < posA);
	CHECK(posD < posB0);
}

TEST_CASE("System - phase dependency cycle uses deterministic fallback") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phaseA = wld.add();
	const auto phaseB = wld.add();

	wld.system().phase(phaseA).all<Position>().on_each([&order](Position) {
		order.push_back('A');
	});
	wld.system().phase(phaseB).all<Position>().on_each([&order](Position) {
		order.push_back('B');
	});

	wld.add(phaseA, {ecs::DependsOn, phaseB});
	wld.add(phaseB, {ecs::DependsOn, phaseA});

	wld.update();

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'A');
		CHECK(order[1] == 'B');
	}
}

TEST_CASE("System - disabled ancestor phase skips descendant phased systems") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phaseRoot = wld.add();
	const auto phaseChild = wld.add();
	wld.child(phaseChild, phaseRoot);

	wld.system().phase(phaseChild).all<Position>().on_each([&order](Position) {
		order.push_back('C');
	});
	wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('U');
	});

	wld.update();
	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'U');
	}

	wld.enable(phaseRoot, false);
	order.clear();
	wld.update();

	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 'U');
}

TEST_CASE("System - dependency added after warm update reorders next run") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto sysRoot = wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('R');
	});
	auto sysChild = wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('C');
	});

	wld.update();
	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'R');
		CHECK(order[1] == 'C');
	}

	wld.add(sysChild.entity(), {ecs::DependsOn, sysRoot.entity()});
	order.clear();
	wld.update();

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'R');
	}
}

TEST_CASE("System - removing System component after warm update drops it from scheduling") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto sysA = wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('A');
	});
	auto sysB = wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('B');
	});

	wld.add(sysB.entity(), {ecs::DependsOn, sysA.entity()});
	wld.update();
	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'B');
		CHECK(order[1] == 'A');
	}

	wld.del<ecs::System_>(sysB.entity());
	order.clear();
	wld.update();

	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 'A');
}

TEST_CASE("System - new system added after warm update is collected on next run") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	auto sysRoot = wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('R');
	});

	wld.update();
	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 'R');

	auto sysChild = wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('C');
	});
	wld.add(sysChild.entity(), {ecs::DependsOn, sysRoot.entity()});
	order.clear();
	wld.update();

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'R');
	}
}

TEST_CASE("System - phase assignment can be moved after warm update") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phaseRoot = wld.add();
	const auto phaseChild = wld.add();
	wld.add(phaseChild, {ecs::DependsOn, phaseRoot});

	auto sys = wld.system().phase(phaseRoot).all<Position>().on_each([&order](Position) {
		order.push_back('S');
	});
	wld.system().phase(phaseChild).all<Position>().on_each([&order](Position) {
		order.push_back('C');
	});

	wld.update();
	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'S');
	}

	wld.del(sys.entity(), {ecs::ChildOf, phaseRoot});
	wld.del(sys.entity(), {ecs::DependsOn, phaseRoot});
	wld.add(sys.entity(), {ecs::ChildOf, phaseChild});
	wld.add(sys.entity(), {ecs::DependsOn, phaseChild});
	order.clear();
	wld.update();

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'S');
		CHECK(order[1] == 'C');
	}
}

TEST_CASE("System - systems_run(phase) runs only that phase") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phaseA = wld.add();
	const auto phaseB = wld.add();
	wld.add(phaseA, {ecs::DependsOn, phaseB});

	auto sysA = wld.system().phase(phaseA).all<Position>().on_each([&order](Position) {
		order.push_back('A');
	});
	auto sysC = wld.system().phase(phaseA).all<Position>().on_each([&order](Position) {
		order.push_back('C');
	});
	wld.system().phase(phaseB).all<Position>().on_each([&order](Position) {
		order.push_back('B');
	});
	wld.system().all<Position>().on_each([&order](Position) {
		order.push_back('U');
	});
	wld.add(sysC.entity(), {ecs::DependsOn, sysA.entity()});

	const auto phaseNested = wld.add();
	wld.child(phaseNested, phaseA);
	wld.system().phase(phaseNested).all<Position>().on_each([&order](Position) {
		order.push_back('N');
	});

	const auto orphan = wld.add();
	wld.child(orphan, phaseA);

	wld.systems_run(phaseA);
	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'A');
	}

	order.clear();
	wld.systems_run(phaseNested);
	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 'N');

	order.clear();
	wld.systems_run(phaseB);
	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 'B');

	order.clear();
	wld.systems_run();
	CHECK(order.size() == 5);
	if (order.size() == 5) {
		CHECK(order[0] == 'C');
		CHECK(order[1] == 'A');
		CHECK(order[2] == 'B');
		CHECK(order[3] == 'N');
		CHECK(order[4] == 'U');
	}
}

TEST_CASE("System - sequential systems_run(phase) commits commands for a later phase") {
	struct PhaseLocalSource {};
	struct PhaseLocalResult {};

	TestWorld twld;
	(void)wld.add<PhaseLocalSource>();
	(void)wld.add<PhaseLocalResult>();

	const auto producerPhase = wld.add();
	const auto consumerPhase = wld.add();
	wld.add(producerPhase, {ecs::DependsOn, consumerPhase});

	const auto e = wld.add();
	wld.add<PhaseLocalSource>(e);

	uint32_t producerHits = 0;
	uint32_t consumerHits = 0;

	wld.system().phase(producerPhase).all<PhaseLocalSource>().on_each([&](ecs::Iter& it) {
		auto& cb = it.cmd_buffer_st();
		auto ev = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			cb.add<PhaseLocalResult>(ev[i]);
			++producerHits;
		}
	});

	wld.system().phase(consumerPhase).all<PhaseLocalResult>().on_each([&](ecs::Iter& it) {
		consumerHits += it.size();
	});

	wld.systems_run(producerPhase);
	CHECK(producerHits == 1);
	CHECK(consumerHits == 0);
	CHECK(wld.has<PhaseLocalResult>(e));

	wld.systems_run(consumerPhase);
	CHECK(consumerHits == 1);
}

TEST_CASE("System - systems_run(phase) skips disabled phase and disabled systems") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phase = wld.add();
	auto sysA = wld.system().phase(phase).all<Position>().on_each([&order](Position) {
		order.push_back('A');
	});
	wld.system().phase(phase).all<Position>().on_each([&order](Position) {
		order.push_back('B');
	});

	wld.enable(sysA.entity(), false);
	wld.systems_run(phase);
	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 'B');

	wld.enable(sysA.entity(), true);
	wld.enable(phase, false);
	order.clear();
	wld.systems_run(phase);
	CHECK(order.empty());
}

TEST_CASE("System - systems_run(phase) collects a system added after a warm run") {
	cnt::darr<char> order;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	const auto phase = wld.add();
	wld.system().phase(phase).all<Position>().on_each([&order](Position) {
		order.push_back('A');
	});

	wld.systems_run(phase);
	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 'A');

	wld.system().phase(phase).all<Position>().on_each([&order](Position) {
		order.push_back('B');
	});
	order.clear();
	wld.systems_run(phase);

	CHECK(order.size() == 2);
	if (order.size() == 2) {
		CHECK(order[0] == 'A');
		CHECK(order[1] == 'B');
	}
}

TEST_CASE("System - shrinking a large schedule clears stale scratch rows") {
	cnt::darr<uint32_t> order;
	cnt::darr<ecs::Entity> systems;
	TestWorld twld;

	auto e = wld.add();
	wld.add<Position>(e, {0, 0, 0});

	constexpr uint32_t N = 32;
	GAIA_FOR(N) {
		const uint32_t id = i;
		auto sys = wld.system().all<Position>().on_each([&order, id](Position) {
			order.push_back(id);
		});
		systems.push_back(sys.entity());
	}

	wld.update();
	CHECK(order.size() == N);

	for (uint32_t i = 1; i < systems.size(); ++i)
		wld.del<ecs::System_>(systems[i]);
	order.clear();
	wld.update();

	CHECK(order.size() == 1);
	if (order.size() == 1)
		CHECK(order[0] == 0);
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
		auto posView = it.view_any_mut<Position>(1);
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
		auto posView = it.view_any_mut<PositionSoA>(1);
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

	wld.add(sysSemantic.entity(), {ecs::DependsOn, sysDirect.entity()});
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
