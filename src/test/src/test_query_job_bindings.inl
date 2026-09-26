//! Prepared inherited binding shape and row-offset regressions. Included by test_mt.cpp.
namespace {
	struct JobBindingInput {
		GAIA_LAYOUT(SoA);
		float x, y;
	};
	struct JobBindingLocal {
		GAIA_LAYOUT(SoA);
		float x, y;
	};
	struct JobBindingOutput {
		int value;
	};
	struct JobBindingSparse {
		GAIA_STORAGE(Sparse);
		int value;
	};
} // namespace

TEST_CASE("Query job bindings - chunk shape and absolute SoA rows") {
	for (auto mode: {ecs::QueryExecType::Default, ecs::QueryExecType::Parallel}) {
		ecs::World world;
		world.add(world.add<JobBindingInput>().entity, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
		ecs::Entity bases[3];
		for (uint32_t i = 0; i < 3; ++i) {
			bases[i] = world.prefab();
			world.add<JobBindingInput>(bases[i], {(float)(i + 1), 0});
		}
		cnt::darray<ecs::Entity> children;
		for (uint32_t i = 0; i < 192; ++i) {
			auto e = world.instantiate(bases[i % 3]);
			world.add<JobBindingLocal>(e, {(float)(i * 10), 0});
			world.add<JobBindingOutput>(e, {-1});
			children.push_back(e);
		}
		// Disabled leading rows force nonzero prepared range offsets.
		for (uint32_t i = 0; i < 3; ++i)
			world.enable(children[i], false);
		auto query = world.query().all<JobBindingInput>().all<JobBindingLocal>().all<JobBindingOutput&>();
		auto job = query.job(
				[](ecs::Entity, JobBindingInput inherited, JobBindingLocal local, JobBindingOutput& result) {
					result.value = (int)(inherited.x + local.x);
				},
				mode);
		CHECK(job.valid());
		if (!job.valid())
			return;
#if GAIA_ECS_TEST_HOOKS
		CHECK(ecs::detail::QueryImpl::test_job_binding_count == 12);
#endif
		job.submit();
		job.wait();
		for (uint32_t i = 0; i < children.size(); ++i)
			CHECK(world.get<JobBindingOutput>(children[i]).value == (i < 3 ? -1 : (int)(i * 10 + i % 3 + 1)));
	}
}

TEST_CASE("Query job bindings - sparse constant owners and row overrides") {
	for (bool nonFragmenting: {false, true}) {
		for (auto mode: {ecs::QueryExecType::Default, ecs::QueryExecType::Parallel}) {
			ecs::World world;
			const auto id = world.add<JobBindingSparse>().entity;
			world.add(id, ecs::Pair(ecs::OnInstantiate, ecs::Inherit));
			if (nonFragmenting)
				world.add(id, ecs::DontFragment);
			ecs::Entity bases[3];
			for (uint32_t i = 0; i < 3; ++i) {
				bases[i] = world.prefab();
				world.add<JobBindingSparse>(bases[i], {(int)i + 1});
			}
			cnt::darray<ecs::Entity> children;
			for (uint32_t i = 0; i < 192; ++i) {
				auto e = world.instantiate(bases[i % 3]);
				world.add<JobBindingOutput>(e, {-1});
				if (i % 4 == 0)
					world.add<JobBindingSparse>(e, {(int)i + 100});
				children.push_back(e);
			}
			auto query = world.query().all<const JobBindingSparse>().all<JobBindingOutput&>();
			auto job = query.job(
					[](const JobBindingSparse& input, JobBindingOutput& result) {
						result.value = input.value;
					},
					mode);
			CHECK(job.valid());
			if (!job.valid())
				return;
#if GAIA_ECS_TEST_HOOKS
			// Table output bindings must not be expanded alongside sparse inputs.
			CHECK(ecs::detail::QueryImpl::test_job_binding_count < 220);
#endif
			world.set<JobBindingSparse>(bases[1]) = {23};
			job.submit();
			job.wait();
			for (uint32_t i = 0; i < children.size(); ++i)
				CHECK(
						world.get<JobBindingOutput>(children[i]).value ==
						(i % 4 == 0 ? (int)i + 100 : (i % 3 == 1 ? 23 : (int)(i % 3) + 1)));
		}
	}
}
