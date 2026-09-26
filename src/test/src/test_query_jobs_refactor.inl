//! Focused prepared Query::job regressions. Included by test_mt.cpp.
namespace {
	struct JobRefactorValue {
		int value;
	};
	struct JobRefactorSparse {
		GAIA_STORAGE(Sparse);
		int value;
	};
} // namespace

TEST_CASE("Query job refactor - default snapshots matching") {
	ecs::World world;
	auto entity = world.add();
	world.add<JobRefactorValue>(entity, {1});
	auto query = world.query().all<JobRefactorValue&>();
	auto job = query.job([](JobRefactorValue& value) {
		++value.value;
	});
	CHECK(job.valid());
	if (!job.valid())
		return;
	CHECK(world.get<JobRefactorValue>(entity).value == 1);
	job.submit();
	job.wait();
	CHECK(world.get<JobRefactorValue>(entity).value == 2);
}

TEST_CASE("Query job refactor - sparse entity seeds") {
	ecs::World world;
	world.add(world.add<JobRefactorSparse>().entity, ecs::DontFragment);
	auto entity = world.add();
	world.add<JobRefactorSparse>(entity, {7});
	auto query = world.query().all<const JobRefactorSparse>();
	std::atomic_uint count{0};
	auto job = query.job(
			[&](const JobRefactorSparse& value) {
				if (value.value == 7)
					count.fetch_add(1, std::memory_order_relaxed);
			},
			ecs::QueryExecType::Parallel);
	CHECK(job.valid());
	if (!job.valid())
		return;
	CHECK(count.load() == 0);
	job.submit();
	job.wait();
	CHECK(count.load() == 1);
}

TEST_CASE("Query job refactor - selected group iterator metadata") {
	ecs::World world;
	auto parent = world.add();
	auto entity = world.add();
	world.add<JobRefactorValue>(entity, {3});
	world.add(entity, ecs::Pair(ecs::ChildOf, parent));
	auto query = world.query().all<const JobRefactorValue>().group_by(ecs::ChildOf).group_id(parent);
	uint32_t count = 0;
	auto job = query.job([&](ecs::Iter& it) {
		CHECK(it.group_id() == parent.id());
		count += it.size();
	});
	CHECK(job.valid());
	if (!job.valid())
		return;
	job.submit();
	job.wait();
	CHECK(count == 1);
}

TEST_CASE("Query job refactor - inherited typed reads") {
	for (auto mode:
			 {ecs::QueryExecType::Default, ecs::QueryExecType::Parallel, ecs::QueryExecType::ParallelPerf,
				ecs::QueryExecType::ParallelEff}) {
		ecs::World world;
		const auto id = world.add<JobRefactorValue>().entity;
		world.add(id, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const auto base = world.prefab();
		world.add<JobRefactorValue>(base, {37});
		const auto child = world.instantiate(base);
		auto query = world.query().all<const JobRefactorValue>();
		std::atomic_int sum{0};
		auto job = query.job(
				[&](ecs::Entity e, const JobRefactorValue& value) {
					if (e == child)
						sum.fetch_add(value.value, std::memory_order_relaxed);
				},
				mode);
		CHECK(job.valid());
		if (!job.valid())
			return;
		CHECK(sum.load() == 0);
		job.submit();
		job.wait();
		CHECK(sum.load() == 37);
		CHECK_FALSE(world.has_direct(child, id));
	}
}

TEST_CASE("Query job refactor - inherited sparse missing writes rejected") {
	for (bool nonFragmenting: {false, true}) {
		ecs::World world;
		const auto id = world.add<JobRefactorSparse>().entity;
		if (nonFragmenting)
			world.add(id, ecs::DontFragment);
		world.add(id, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const auto base = world.prefab();
		world.add<JobRefactorSparse>(base, {37});
		const auto child = world.instantiate(base);
		auto query = world.query().all<JobRefactorSparse&>();
		auto job = query.job(
				[](JobRefactorSparse& value) {
					++value.value;
				},
				ecs::QueryExecType::Parallel);
		CHECK_FALSE(job.valid());
		CHECK_FALSE(world.has_direct(child, id));
		CHECK(world.get<JobRefactorSparse>(base).value == 37);
	}
}

TEST_CASE("Query job refactor - inherited sparse prepared payloads") {
	for (bool nonFragmenting: {false, true}) {
		for (auto mode:
				 {ecs::QueryExecType::Default, ecs::QueryExecType::Parallel, ecs::QueryExecType::ParallelPerf,
					ecs::QueryExecType::ParallelEff}) {
			ecs::World world;
			const auto id = world.add<JobRefactorSparse>().entity;
			if (nonFragmenting)
				world.add(id, ecs::DontFragment);
			world.add(id, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
			const auto first = world.prefab();
			const auto second = world.prefab();
			world.add<JobRefactorSparse>(first, {11});
			world.add<JobRefactorSparse>(second, {23});
			cnt::darray<ecs::Entity> children;
			for (int i = 0; i < 96; ++i) {
				const auto child = world.instantiate(i % 2 == 0 ? first : second);
				world.add<JobRefactorValue>(child, {0});
				if (i % 3 == 0)
					world.add<JobRefactorSparse>(child, {41});
				children.push_back(child);
			}
			auto query = world.query().all<const JobRefactorSparse>().all<JobRefactorValue&>();
			auto job = query.job(
					[](const JobRefactorSparse& input, JobRefactorValue& output) {
						output.value = input.value;
					},
					mode);
			CHECK(job.valid());
			if (!job.valid())
				return;
			// Bindings refer to live payloads, not snapshots of the component values.
			world.set<JobRefactorSparse>(first) = {13};
			job.submit();
			job.wait();
			for (uint32_t i = 0; i < children.size(); ++i) {
				CHECK(world.get<JobRefactorValue>(children[i]).value == (i % 3 == 0 ? 41 : (i % 2 == 0 ? 13 : 23)));
				CHECK(world.has_direct(children[i], id) == (i % 3 == 0));
			}
			CHECK(world.get<JobRefactorSparse>(first).value == 13);
			CHECK(world.get<JobRefactorSparse>(second).value == 23);
		}
	}
}

TEST_CASE("Query job refactor - inherited and direct SoA bindings preserve rows") {
	for (auto mode: {ecs::QueryExecType::Default, ecs::QueryExecType::Parallel}) {
		ecs::World world;
		const auto component = world.add<PositionSoA>().entity;
		world.add(component, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const ecs::Entity bases[] = {world.prefab(), world.prefab()};
		for (uint32_t i = 0; i < 2; ++i)
			world.add<PositionSoA>(bases[i], {(float)(i + 1), 0, 0});
		CHECK(world.fetch(bases[1]).row != 0);
		ecs::Entity children[16];
		for (uint32_t i = 0; i < 16; ++i) {
			children[i] = world.instantiate(bases[i % 2]);
			world.add<RotationSoA>(children[i], {(float)(i * 10), 0, 0, 0});
			world.add<JobRefactorValue>(children[i], {0});
		}
		auto query = world.query().all<PositionSoA>().all<RotationSoA>().all<JobRefactorValue&>();
		auto job = query.job(
				[](PositionSoA inherited, RotationSoA local, JobRefactorValue& result) {
					result.value = (int)(inherited.x + local.x);
				},
				mode);
		CHECK(job.valid());
		if (!job.valid())
			return;
		job.submit();
		job.wait();
		for (uint32_t i = 0; i < 16; ++i)
			CHECK(world.get<JobRefactorValue>(children[i]).value == (int)(i * 10 + i % 2 + 1));
	}
}

TEST_CASE("Query job refactor - inherited writes require existing overrides") {
	for (auto mode:
			 {ecs::QueryExecType::Default, ecs::QueryExecType::Parallel, ecs::QueryExecType::ParallelPerf,
				ecs::QueryExecType::ParallelEff}) {
		ecs::World world;
		const auto table = world.add<JobRefactorValue>().entity;
		const auto sparse = world.add<JobRefactorSparse>().entity;
		world.add(sparse, ecs::DontFragment);
		world.add(table, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		world.add(sparse, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		const auto base = world.prefab();
		world.add<JobRefactorValue>(base, {2});
		world.add<JobRefactorSparse>(base, {3});
		const auto child = world.instantiate(base);
		auto tableQuery = world.query().all<JobRefactorValue&>();
		auto sparseQuery = world.query().all<JobRefactorSparse&>();
		auto rejectedTable = tableQuery.job(
				[](JobRefactorValue& v) {
					++v.value;
				},
				mode);
		auto rejectedSparse = sparseQuery.job(
				[](JobRefactorSparse& v) {
					++v.value;
				},
				mode);
		CHECK_FALSE(rejectedTable.valid());
		CHECK_FALSE(rejectedSparse.valid());
		CHECK_FALSE(world.has_direct(child, table));
		CHECK_FALSE(world.has_direct(child, sparse));
		world.override<JobRefactorValue>(child);
		world.override<JobRefactorSparse>(child);
		auto query = world.query().all<JobRefactorValue&>().all<JobRefactorSparse&>();
		auto job = query.job(
				[](JobRefactorValue& a, JobRefactorSparse& b) {
					++a.value;
					++b.value;
				},
				mode);
		CHECK(job.valid());
		if (!job.valid())
			return;
		job.submit();
		job.wait();
		CHECK(world.get<JobRefactorValue>(child).value == 3);
		CHECK(world.get<JobRefactorSparse>(child).value == 4);
		CHECK(world.get<JobRefactorValue>(base).value == 2);
		CHECK(world.get<JobRefactorSparse>(base).value == 3);
	}
}
