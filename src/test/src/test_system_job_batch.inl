//! Included by a test translation unit that provides test_common.h.
TEST_CASE("System - query job batch completes before later preparation") {
	struct BatchSource {};
	struct BatchResult {};

	ecs::World world;
	(void)world.add<BatchSource>();
	(void)world.add<BatchResult>();
	const auto entity = world.add();
	world.add<BatchSource>(entity);
	uint32_t producerHits = 0;
	uint32_t consumerHits = 0;

	auto producer = world.system().all<BatchSource>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		auto& cmds = it.cmd_buffer_st();
		auto entities = it.view<ecs::Entity>();
		GAIA_EACH(it) {
			cmds.add<BatchResult>(entities[i]);
			++producerHits;
		}
	});
	auto consumer = world.system().all<BatchResult>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		consumerHits += it.size();
	});

	SUBCASE("access conflict") {
		producer.writes<BatchResult>();
		consumer.reads<BatchResult>();
	}
	SUBCASE("dependency depth") {
		world.add(producer.entity(), ecs::Pair(ecs::DependsOn, consumer.entity()));
	}
	SUBCASE("phase") {
		const auto producerPhase = world.add();
		const auto consumerPhase = world.add();
		world.add(producerPhase, ecs::Pair(ecs::DependsOn, consumerPhase));
		producer.phase(producerPhase);
		consumer.phase(consumerPhase);
	}
	SUBCASE("serial consumer") {
		consumer.mode(ecs::QueryExecType::Default);
	}
	SUBCASE("main thread consumer") {
		consumer.main_thread();
	}

	world.systems_run();
	CHECK(producerHits == 1);
	CHECK(consumerHits == 1);
	CHECK(world.has<BatchResult>(entity));
}
