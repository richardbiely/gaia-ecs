#include "test_common.h"

#include <chrono>

//------------------------------------------------------------------------------
// Multithreading
//------------------------------------------------------------------------------

struct JobParallelRefProbe {
	std::atomic_uint32_t items = 0;
	std::atomic_uint32_t batches = 0;

	static void invoke(void* pCtx, const mt::JobArgs& args) {
		auto& ctx = *(JobParallelRefProbe*)pCtx;
		ctx.items.fetch_add(args.idxEnd - args.idxStart, std::memory_order_relaxed);
		ctx.batches.fetch_add(1, std::memory_order_relaxed);
	}
};

struct ExternalExecProbeComp {
	uint32_t value = 0;
};

struct ExternalSchedProbe {
	ecs::SchedParDesc addedParallel[8]{};
	ecs::SchedTaskDesc addedTasks[8]{};
	uint32_t runParallelCalls = 0;
	uint32_t addTaskCalls = 0;
	uint32_t addParallelCalls = 0;
	uint32_t submitCalls = 0;
	uint32_t depCalls = 0;
	uint32_t waitCalls = 0;
	uint32_t delCalls = 0;
	uint32_t invokeCalls = 0;
	uint32_t itemsProcessed = 0;
	ecs::QueryExecType lastExecType = ecs::QueryExecType::Default;
	uint32_t lastItemCount = 0;
	uint32_t lastGroupSize = 0;
	ecs::SchedToken depFirst[8]{};
	ecs::SchedToken depSecond[8]{};

	static ecs::SchedToken run_parallel(void* pCtx, const ecs::SchedParDesc* pDesc) {
		auto& probe = *(ExternalSchedProbe*)pCtx;
		++probe.runParallelCalls;
		probe.lastExecType = pDesc->execType;
		probe.lastItemCount = pDesc->itemCount;
		probe.lastGroupSize = pDesc->groupSize;

		const auto mid = pDesc->itemCount > 1 ? pDesc->itemCount / 2 : pDesc->itemCount;
		if (mid != 0) {
			pDesc->invoke(pDesc->pCtx, 0, mid);
			++probe.invokeCalls;
			probe.itemsProcessed += mid;
		}
		if (mid < pDesc->itemCount) {
			pDesc->invoke(pDesc->pCtx, mid, pDesc->itemCount);
			++probe.invokeCalls;
			probe.itemsProcessed += pDesc->itemCount - mid;
		}

		ecs::SchedToken token{};
		token.value[0] = probe.runParallelCalls;
		token.value[1] = pDesc->itemCount;
		return token;
	}

	static ecs::SchedToken add_task(void* pCtx, const ecs::SchedTaskDesc* pDesc) {
		auto& probe = *(ExternalSchedProbe*)pCtx;
		++probe.addTaskCalls;
		probe.addedTasks[probe.addTaskCalls - 1] = *pDesc;

		ecs::SchedToken token{};
		token.value[0] = probe.addTaskCalls;
		token.value[1] = 0;
		return token;
	}

	static ecs::SchedToken add_parallel(void* pCtx, const ecs::SchedParDesc* pDesc) {
		auto& probe = *(ExternalSchedProbe*)pCtx;
		++probe.addParallelCalls;
		probe.lastExecType = pDesc->execType;
		probe.lastItemCount = pDesc->itemCount;
		probe.lastGroupSize = pDesc->groupSize;
		probe.addedParallel[probe.addParallelCalls - 1] = *pDesc;

		ecs::SchedToken token{};
		token.value[0] = probe.addParallelCalls;
		token.value[1] = pDesc->itemCount;
		return token;
	}

	static void submit(void* pCtx, ecs::SchedToken token) {
		auto& probe = *(ExternalSchedProbe*)pCtx;
		++probe.submitCalls;
		if (token.value[1] == 0) {
			auto& desc = probe.addedTasks[(uint32_t)token.value[0] - 1];
			desc.invoke(desc.pCtx);
			++probe.invokeCalls;
			return;
		}

		auto& desc = probe.addedParallel[(uint32_t)token.value[0] - 1];
		const auto mid = desc.itemCount > 1 ? desc.itemCount / 2 : desc.itemCount;
		if (mid != 0) {
			desc.invoke(desc.pCtx, 0, mid);
			++probe.invokeCalls;
			probe.itemsProcessed += mid;
		}
		if (mid < desc.itemCount) {
			desc.invoke(desc.pCtx, mid, desc.itemCount);
			++probe.invokeCalls;
			probe.itemsProcessed += desc.itemCount - mid;
		}
	}

	static void dep(void* pCtx, ecs::SchedToken tokenFirst, ecs::SchedToken tokenSecond) {
		auto& probe = *(ExternalSchedProbe*)pCtx;
		probe.depFirst[probe.depCalls] = tokenFirst;
		probe.depSecond[probe.depCalls] = tokenSecond;
		++probe.depCalls;
	}

	static void wait(void* pCtx, ecs::SchedToken) {
		auto& probe = *(ExternalSchedProbe*)pCtx;
		++probe.waitCalls;
	}

	static void del(void* pCtx, ecs::SchedToken) {
		auto& probe = *(ExternalSchedProbe*)pCtx;
		++probe.delCalls;
	}

	GAIA_NODISCARD ecs::Sched sched() {
		ecs::Sched sched{};
		sched.pCtx = this;
		sched.sched_par = &ExternalSchedProbe::run_parallel;
		sched.add = &ExternalSchedProbe::add_task;
		sched.add_par = &ExternalSchedProbe::add_parallel;
		sched.submit = &ExternalSchedProbe::submit;
		sched.dep = &ExternalSchedProbe::dep;
		sched.wait = &ExternalSchedProbe::wait;
		sched.del = &ExternalSchedProbe::del;
		return sched;
	}
};

TEST_CASE("ECS - Query jobs use external scheduler wrappers") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 29;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<ExternalExecProbeComp>(e, {1});
	}

	uint32_t hitsA = 0;
	uint32_t hitsB = 0;
	auto queryA = wld.query().all<ExternalExecProbeComp>();
	auto queryB = wld.query().all<ExternalExecProbeComp>();

	auto jobA = queryA.job(
			[&](ecs::Iter& it) {
				hitsA += it.entity_rows().size();
			},
			ecs::QueryExecType::Default);
	auto jobB = queryB.job(
			[&](ecs::Iter& it) {
				hitsB += it.entity_rows().size();
			},
			ecs::QueryExecType::Default);

	CHECK(probe.addTaskCalls == 2);
	CHECK(probe.addParallelCalls == 0);
	CHECK(probe.submitCalls == 0);
	CHECK(probe.waitCalls == 0);
	CHECK(probe.delCalls == 0);
	CHECK(hitsA == 0);
	CHECK(hitsB == 0);

	jobA.submit();
	jobB.submit();
	CHECK(probe.submitCalls == 2);
	CHECK(hitsA == EntityCount);
	CHECK(hitsB == EntityCount);

	jobA.wait();
	jobB.wait();
	jobA.del();
	jobB.del();

	CHECK(probe.waitCalls == 2);
	CHECK(probe.delCalls == 2);
}

TEST_CASE("ECS - Parallel query jobs add scheduler work without running it") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 31;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<ExternalExecProbeComp>(e, {1});
	}

	uint32_t hitsA = 0;
	uint32_t hitsB = 0;
	auto queryA = wld.query().all<ExternalExecProbeComp>();
	auto queryB = wld.query().all<ExternalExecProbeComp>();

	auto jobA = queryA.job(
			[&](ecs::Iter& it) {
				hitsA += it.entity_rows().size();
			},
			ecs::QueryExecType::Parallel);
	auto jobB = queryB.job(
			[&](ecs::Iter& it) {
				hitsB += it.entity_rows().size();
			},
			ecs::QueryExecType::Parallel);

	CHECK(probe.addTaskCalls == 0);
	CHECK(probe.addParallelCalls == 2);
	CHECK(probe.submitCalls == 0);
	CHECK(probe.waitCalls == 0);
	CHECK(probe.delCalls == 0);
	CHECK(hitsA == 0);
	CHECK(hitsB == 0);

	jobB.dep(jobA);
	CHECK(probe.depCalls == 1);

	jobA.submit();
	jobB.submit();
	CHECK(probe.submitCalls == 2);
	CHECK(hitsA == EntityCount);
	CHECK(hitsB == EntityCount);

	jobA.wait();
	jobB.wait();
	jobA.del();
	jobB.del();

	CHECK(probe.waitCalls == 2);
	CHECK(probe.delCalls == 2);
}

TEST_CASE("ECS - Grouped parallel query jobs use scheduler task path") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	const auto groupRel = wld.add();
	const auto selectedGroup = wld.add();
	const auto otherGroup = wld.add();

	constexpr uint32_t SelectedCount = 7;
	constexpr uint32_t OtherCount = 5;
	GAIA_FOR(SelectedCount) {
		auto e = wld.add();
		wld.add<ExternalExecProbeComp>(e, {1});
		wld.add(e, ecs::Pair(groupRel, selectedGroup));
	}
	GAIA_FOR(OtherCount) {
		auto e = wld.add();
		wld.add<ExternalExecProbeComp>(e, {1});
		wld.add(e, ecs::Pair(groupRel, otherGroup));
	}

	uint32_t hits = 0;
	auto query = wld.query().all<ExternalExecProbeComp>().group_by(groupRel);
	query.group_id(selectedGroup);
	auto job = query.job(
			[&](ecs::Iter& it) {
				hits += it.entity_rows().size();
			},
			ecs::QueryExecType::Parallel);

	CHECK(probe.addTaskCalls == 1);
	CHECK(probe.addParallelCalls == 0);
	CHECK(probe.submitCalls == 0);
	CHECK(hits == 0);

	job.submit();
	CHECK(probe.submitCalls == 1);
	CHECK(probe.invokeCalls >= 1);
	CHECK(hits == SelectedCount);

	job.wait();
	job.del();
	CHECK(probe.waitCalls >= 1);
	CHECK(probe.delCalls >= 1);
}

TEST_CASE("ECS - Typed query jobs add scheduler parallel work without blocking internally") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 27;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<ExternalExecProbeComp>(e, {1});
	}

	uint32_t hits = 0;
	auto query = wld.query().all<ExternalExecProbeComp&>();
	auto job = query.job(
			[&](ExternalExecProbeComp& comp) {
				++hits;
				++comp.value;
			},
			ecs::QueryExecType::Parallel);

	CHECK(probe.addTaskCalls == 0);
	CHECK(probe.addParallelCalls == 1);
	CHECK(probe.runParallelCalls == 0);
	CHECK(probe.submitCalls == 0);
	CHECK(hits == 0);

	job.submit();
	CHECK(probe.submitCalls == 1);
	CHECK(probe.invokeCalls >= 1);
	CHECK(hits == EntityCount);

	job.wait();
	uint32_t mutated = 0;
	wld.query().all<const ExternalExecProbeComp>().each([&](const ExternalExecProbeComp& comp) {
		CHECK(comp.value == 2);
		++mutated;
	});
	CHECK(mutated == EntityCount);

	job.del();
	CHECK(probe.waitCalls == 1);
	CHECK(probe.delCalls == 1);
}

TEST_CASE("ECS - System jobs use external scheduler wrappers") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 23;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<ExternalExecProbeComp>(e, {1});
	}

	uint32_t hits = 0;
	auto sys = wld.system().all<ExternalExecProbeComp>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		hits += it.entity_rows().size();
	});

	auto job = sys.job();
	CHECK(probe.addTaskCalls == 0);
	CHECK(probe.addParallelCalls == 1);
	CHECK(probe.submitCalls == 0);
	CHECK(hits == 0);

	job.submit();
	CHECK(probe.submitCalls == 1);
	CHECK(hits == EntityCount);

	job.wait();
	job.del();
	CHECK(probe.waitCalls == 1);
	CHECK(probe.delCalls == 1);
}

TEST_CASE("ECS - Scheduler descriptors preserve background flags for external schedulers") {
	ExternalSchedProbe probe;
	const auto sched = probe.sched();

	std::atomic_bool taskRan = false;
	ecs::SchedTaskDesc taskDesc{};
	taskDesc.pCtx = &taskRan;
	taskDesc.invoke = [](void* pCtx) {
		((std::atomic_bool*)pCtx)->store(true, std::memory_order_release);
	};
	taskDesc.execType = ecs::QueryExecType::Default;
	taskDesc.flags = ecs::SchedFlags::Background;

	auto taskJob = ecs::sched_add(sched, taskDesc, nullptr, nullptr);
	CHECK(taskJob.valid());
	CHECK(probe.addTaskCalls == 1);
	CHECK(probe.addedTasks[0].flags == ecs::SchedFlags::Background);
	CHECK(ecs::sched_flags_has(probe.addedTasks[0].flags, ecs::SchedFlags::Background));
	CHECK_FALSE(taskRan.load(std::memory_order_acquire));

	taskJob.submit();
	CHECK(taskRan.load(std::memory_order_acquire));
	taskJob.wait();
	taskJob.del();

	std::atomic_uint32_t items = 0;
	ecs::SchedParDesc parDesc{};
	parDesc.pCtx = &items;
	parDesc.invoke = [](void* pCtx, uint32_t idxStart, uint32_t idxEnd) {
		((std::atomic_uint32_t*)pCtx)->fetch_add(idxEnd - idxStart, std::memory_order_release);
	};
	parDesc.itemCount = 4;
	parDesc.groupSize = 2;
	parDesc.execType = ecs::QueryExecType::Parallel;
	parDesc.flags = ecs::SchedFlags::Background;

	auto parJob = ecs::sched_add_par(sched, parDesc, nullptr, nullptr);
	CHECK(parJob.valid());
	CHECK(probe.addParallelCalls == 1);
	CHECK(probe.addedParallel[0].flags == ecs::SchedFlags::Background);
	CHECK(ecs::sched_flags_has(probe.addedParallel[0].flags, ecs::SchedFlags::Background));
	CHECK(items.load(std::memory_order_acquire) == 0);

	parJob.submit();
	CHECK(items.load(std::memory_order_acquire) == parDesc.itemCount);
	parJob.wait();
	parJob.del();

	CHECK(probe.submitCalls == 2);
	CHECK(probe.waitCalls == 2);
	CHECK(probe.delCalls == 2);
}

TEST_CASE("ECS - Default scheduler routes background flags outside frame update") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(2, 2);
	tp.set_background_workers(1);

	struct BackgroundSchedCtx {
		std::atomic_bool release = false;
		std::atomic_bool blockingStarted = false;
		std::atomic_bool blockingDone = false;
		std::atomic_bool blockingCtxOk = false;
		std::atomic_bool queuedRan = false;
		std::atomic_bool queuedCtxOk = false;
	};

	BackgroundSchedCtx ctx;
	ecs::SchedTaskDesc blockingDesc{};
	blockingDesc.pCtx = &ctx;
	blockingDesc.invoke = [](void* pCtx) {
		auto& ctx = *(BackgroundSchedCtx*)pCtx;
		auto* workerCtx = mt::detail::tl_workerCtx;
		ctx.blockingCtxOk.store(workerCtx != nullptr && workerCtx->background, std::memory_order_release);
		ctx.blockingStarted.store(true, std::memory_order_release);
		while (!ctx.release.load(std::memory_order_acquire))
			std::this_thread::yield();
		ctx.blockingDone.store(true, std::memory_order_release);
	};
	blockingDesc.flags = ecs::SchedFlags::Background;

	auto blockingJob = ecs::sched_add(ecs::Sched{}, blockingDesc, nullptr, nullptr);
	blockingJob.submit();

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!ctx.blockingStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
		std::this_thread::yield();

	CHECK(ctx.blockingStarted.load(std::memory_order_acquire));
	CHECK(ctx.blockingCtxOk.load(std::memory_order_acquire));

	ecs::SchedTaskDesc queuedDesc{};
	queuedDesc.pCtx = &ctx;
	queuedDesc.invoke = [](void* pCtx) {
		auto& ctx = *(BackgroundSchedCtx*)pCtx;
		auto* workerCtx = mt::detail::tl_workerCtx;
		ctx.queuedCtxOk.store(workerCtx != nullptr && workerCtx->background, std::memory_order_release);
		ctx.queuedRan.store(true, std::memory_order_release);
	};
	queuedDesc.flags = ecs::SchedFlags::Background;

	auto queuedJob = ecs::sched_add(ecs::Sched{}, queuedDesc, nullptr, nullptr);
	queuedJob.submit();

	std::atomic_bool frameRan = false;
	mt::Job frameJob;
	frameJob.flags = mt::JobCreationFlags::ManualDelete;
	frameJob.func = [&]() {
		frameRan.store(true, std::memory_order_release);
	};

	const auto frameHandle = tp.sched(GAIA_MOV(frameJob));
	tp.wait(frameHandle);
	tp.del(frameHandle);

	GAIA_FOR(64) {
		tp.update();
		std::this_thread::yield();
	}

	CHECK(frameRan.load(std::memory_order_acquire));
	CHECK_FALSE(ctx.blockingDone.load(std::memory_order_acquire));
	CHECK_FALSE(ctx.queuedRan.load(std::memory_order_acquire));

	ctx.release.store(true, std::memory_order_release);
	blockingJob.wait();
	queuedJob.wait();

	CHECK(ctx.blockingDone.load(std::memory_order_acquire));
	CHECK(ctx.queuedRan.load(std::memory_order_acquire));
	CHECK(ctx.queuedCtxOk.load(std::memory_order_acquire));

	blockingJob.del();
	queuedJob.del();
	tp.set_background_workers(0);
}

TEST_CASE("Multithreading - JobHandle") {
	const mt::JobHandle defaultHandle;
	CHECK(defaultHandle.id() == mt::JobHandle::IdMask);
	CHECK(defaultHandle.gen() == mt::JobHandle::GenMask);
	CHECK(defaultHandle.prio() == mt::JobHandle::PrioMask);
	CHECK(defaultHandle == mt::JobNull);
	CHECK_FALSE(defaultHandle != mt::JobNull);

	const mt::JobHandle handle(123, 45, 1);
	CHECK(handle.id() == 123);
	CHECK(handle.gen() == 45);
	CHECK(handle.prio() == 1);
	CHECK(handle != defaultHandle);
	CHECK(handle != mt::JobNull);
	CHECK_FALSE(handle == mt::JobNull);

	const mt::JobHandle handleFromValue((uint32_t)handle.value());
	CHECK(handleFromValue == handle);
	CHECK(handleFromValue.value() == handle.value());
}

TEST_CASE("Multithreading - Event") {
	mt::Event event;
	CHECK_FALSE(event.is_set());

	event.set();
	CHECK(event.is_set());
	event.wait();
	event.reset();
	CHECK_FALSE(event.is_set());

	std::atomic<bool> waiterStarted = false;
	std::atomic<bool> waiterFinished = false;

	std::thread waiter([&]() {
		waiterStarted.store(true, std::memory_order_release);
		event.wait();
		waiterFinished.store(true, std::memory_order_release);
	});

	while (!waiterStarted.load(std::memory_order_acquire)) {
	}
	CHECK_FALSE(waiterFinished.load(std::memory_order_acquire));

	event.set();
	waiter.join();
	CHECK(waiterFinished.load(std::memory_order_acquire));

	event.reset();
	CHECK_FALSE(event.is_set());
}

TEST_CASE("Multithreading - ScheduleParallelRef") {
	auto& tp = mt::ThreadPool::get();
	CHECK(mt::ThreadPool::hw_thread_cnt() >= 1);
	CHECK(mt::ThreadPool::hw_efficiency_cores_cnt() <= mt::ThreadPool::hw_thread_cnt());

	JobParallelRefProbe ctx;
	mt::JobParallelRef jobRef{&ctx, &JobParallelRefProbe::invoke, mt::JobPriority::High};

	tp.set_max_workers(0, 0);
	CHECK(tp.workers() == 0);

	auto handle = tp.sched_par(jobRef, 7, 16);
	tp.wait(handle);
	CHECK(ctx.items.load(std::memory_order_relaxed) == 7);
	CHECK(ctx.batches.load(std::memory_order_relaxed) == 1);

	ctx.items.store(0, std::memory_order_relaxed);
	ctx.batches.store(0, std::memory_order_relaxed);

	handle = tp.sched_par(jobRef, 25, 4);
	tp.wait(handle);
	CHECK(ctx.items.load(std::memory_order_relaxed) == 25);
	CHECK(ctx.batches.load(std::memory_order_relaxed) > 1);

	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	ctx.items.store(0, std::memory_order_relaxed);
	ctx.batches.store(0, std::memory_order_relaxed);

	handle = tp.sched_par(jobRef, 32, 0);
	tp.wait(handle);
	CHECK(ctx.items.load(std::memory_order_relaxed) == 32);
	CHECK(ctx.batches.load(std::memory_order_relaxed) >= 1);
}

TEST_CASE("ECS - Query uses external scheduler") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 37;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<ExternalExecProbeComp>(e, {i});
	}

	uint32_t sum = 0;
	auto q = wld.query().all<ExternalExecProbeComp>();
	q.each(
			[&](const ExternalExecProbeComp& comp) {
				sum += comp.value;
			},
			ecs::QueryExecType::ParallelEff);

	CHECK(probe.runParallelCalls == 1);
	CHECK(probe.waitCalls == 1);
	CHECK(probe.delCalls == 1);
	CHECK(probe.invokeCalls >= 1);
	CHECK(probe.itemsProcessed == probe.lastItemCount);
	CHECK(probe.lastExecType == ecs::QueryExecType::ParallelEff);
	CHECK(probe.lastItemCount >= 1);
	CHECK(sum == (EntityCount - 1) * EntityCount / 2);
}

TEST_CASE("ECS - Systems use external scheduler") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 19;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<ExternalExecProbeComp>(e, {1});
	}

	uint32_t hits = 0;
	wld.system()
			.all<ExternalExecProbeComp>()
			.mode(ecs::QueryExecType::Parallel)
			.on_each([&](const ExternalExecProbeComp&) {
				++hits;
			});

	wld.update();

	CHECK(probe.runParallelCalls == 0);
	CHECK(probe.addTaskCalls == 0);
	CHECK(probe.addParallelCalls == 1);
	CHECK(probe.submitCalls == 1);
	CHECK(probe.waitCalls == 1);
	CHECK(probe.delCalls == 1);
	CHECK(probe.lastExecType == ecs::QueryExecType::Parallel);
	CHECK(probe.itemsProcessed == probe.lastItemCount);
	CHECK(probe.lastItemCount >= 1);
	CHECK(hits == EntityCount);
}

TEST_CASE("ECS - System update wires parallel job dependencies from query access") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 17;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<Position>(e, {float(i), 0.0F, 0.0F});
		wld.add<Acceleration>(e, {1.0F, 0.0F, 0.0F});
	}

	uint32_t posWritesA = 0;
	uint32_t posWritesB = 0;
	uint32_t accelReads = 0;
	wld.system().all<Position&>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		posWritesA += it.entity_rows().size();
	});
	wld.system().all<Position&>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		posWritesB += it.entity_rows().size();
	});
	wld.system().all<const Acceleration>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		accelReads += it.entity_rows().size();
	});

	wld.update();

	CHECK(probe.addParallelCalls == 3);
	CHECK(probe.submitCalls == 3);
	CHECK(probe.waitCalls == 3);
	CHECK(probe.delCalls == 3);
	CHECK(probe.depCalls == 1);
	CHECK(probe.depFirst[0].value[0] == 1);
	CHECK(probe.depSecond[0].value[0] == 2);
	CHECK(posWritesA == EntityCount);
	CHECK(posWritesB == EntityCount);
	CHECK(accelReads == EntityCount);
}

TEST_CASE("ECS - System update wires custom access dependencies") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 13;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<Position>(e, {float(i), 0.0F, 0.0F});
	}

	uint32_t writerHits = 0;
	uint32_t readerHits = 0;
	uint32_t independentHits = 0;
	wld.system()
			.all<const Position>()
			.writes<Acceleration>()
			.mode(ecs::QueryExecType::Parallel)
			.on_each([&](ecs::Iter& it) {
				writerHits += it.entity_rows().size();
			});
	wld.system()
			.all<const Position>()
			.reads<Acceleration>()
			.mode(ecs::QueryExecType::Parallel)
			.on_each([&](ecs::Iter& it) {
				readerHits += it.entity_rows().size();
			});
	wld.system().all<const Position>().reads<Scale>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		independentHits += it.entity_rows().size();
	});

	wld.update();

	CHECK(probe.addParallelCalls == 3);
	CHECK(probe.submitCalls == 3);
	CHECK(probe.depCalls == 1);
	CHECK(probe.depFirst[0].value[0] == 1);
	CHECK(probe.depSecond[0].value[0] == 2);
	CHECK(writerHits == EntityCount);
	CHECK(readerHits == EntityCount);
	CHECK(independentHits == EntityCount);
}

TEST_CASE("ECS - System update treats phase boundaries as depth-first job barriers") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 7;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<Position>(e, {float(i), 0.0F, 0.0F});
		wld.add<Acceleration>(e, {1.0F, 0.0F, 0.0F});
	}

	const auto phaseA = wld.add();
	const auto phaseB = wld.add();
	wld.add(phaseB, {ecs::DependsOn, phaseA});

	uint32_t phaseAHits = 0;
	uint32_t phaseBHits = 0;
	wld.system().phase(phaseA).all<Position&>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		CHECK(phaseBHits == EntityCount);
		phaseAHits += it.entity_rows().size();
	});
	wld.system().phase(phaseB).all<const Acceleration>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		CHECK(probe.submitCalls == 1);
		CHECK(probe.waitCalls == 0);
		CHECK(probe.delCalls == 0);
		CHECK(phaseAHits == 0);
		phaseBHits += it.entity_rows().size();
	});

	wld.update();

	CHECK(probe.addParallelCalls == 2);
	CHECK(probe.submitCalls == 2);
	CHECK(probe.waitCalls == 2);
	CHECK(probe.delCalls == 2);
	CHECK(probe.depCalls == 0);
	CHECK(phaseAHits == EntityCount);
	CHECK(phaseBHits == EntityCount);
}

TEST_CASE("ECS - System update treats main-thread systems as job barriers") {
	TestWorld twld;
	ExternalSchedProbe probe;
	wld.set_sched(probe.sched());

	constexpr uint32_t EntityCount = 11;
	GAIA_FOR(EntityCount) {
		auto e = wld.add();
		wld.add<Position>(e, {float(i), 0.0F, 0.0F});
		wld.add<Acceleration>(e, {1.0F, 0.0F, 0.0F});
	}

	uint32_t beforeHits = 0;
	uint32_t mainHits = 0;
	uint32_t afterHits = 0;
	wld.system().all<Position&>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		beforeHits += it.entity_rows().size();
	});
	wld.system().all<const Position>().main_thread().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		mainHits += it.entity_rows().size();
		CHECK(probe.submitCalls == 1);
		CHECK(probe.waitCalls == 1);
		CHECK(probe.delCalls == 1);
	});
	wld.system().all<const Acceleration>().mode(ecs::QueryExecType::Parallel).on_each([&](ecs::Iter& it) {
		afterHits += it.entity_rows().size();
	});

	wld.update();

	CHECK(probe.addParallelCalls == 2);
	CHECK(probe.submitCalls == 2);
	CHECK(probe.waitCalls == 3);
	CHECK(probe.delCalls == 3);
	CHECK(probe.depCalls == 0);
	CHECK(beforeHits == EntityCount);
	CHECK(mainHits == EntityCount);
	CHECK(afterHits == EntityCount);
}

template <typename TQueue>
void TestJobQueue_PushPopSteal(bool reverse) {
	mt::JobHandle handle;
	TQueue q;
	bool res;

	for (uint32_t i = 0; i < 5; ++i) {
		CHECK(q.empty());
		res = q.try_push(mt::JobHandle(1, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_push(mt::JobHandle(2, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_push(mt::JobHandle(3, 0, 0));
		CHECK(res);
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		mt::JobHandle handles[3] = {mt::JobHandle(1, 0, 0), mt::JobHandle(2, 0, 0), mt::JobHandle(3, 0, 0)};
		res = q.try_push(std::span(handles, 3));
		CHECK(res);
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		CHECK(q.empty());
		(void)q.try_push(mt::JobHandle(1, 0, 0));
		CHECK_FALSE(q.empty());
		(void)q.try_push(mt::JobHandle(2, 0, 0));
		CHECK_FALSE(q.empty());

		res = q.try_pop(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_pop(handle);
		CHECK(res);
		CHECK(q.empty());
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_pop(handle);
		CHECK_FALSE(res);
		CHECK(q.empty());
	}

	{
		(void)q.try_push(mt::JobHandle(1, 0, 0));
		CHECK_FALSE(q.empty());
		(void)q.try_push(mt::JobHandle(2, 0, 0));
		CHECK_FALSE(q.empty());

		res = q.try_steal(handle);
		CHECK(res);
		CHECK_FALSE(q.empty());
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_steal(handle);
		CHECK(res);
		CHECK(q.empty());
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_steal(handle);
		CHECK(res);
		CHECK(handle == (mt::JobHandle)mt::JobNull_t{});
		CHECK(q.empty());
	}
}

template <typename TQueue>
void TestJobQueue_PushPop(bool reverse) {
	mt::JobHandle handle;
	TQueue q;

	{
		q.try_push(mt::JobHandle(1, 0, 0));
		q.try_push(mt::JobHandle(2, 0, 0));

		auto res = q.try_pop(handle);
		CHECK(res);
		if (reverse)
			CHECK(handle.id() == 1);
		else
			CHECK(handle.id() == 2);

		res = q.try_pop(handle);
		CHECK(res);
		if (reverse)
			CHECK(handle.id() == 2);
		else
			CHECK(handle.id() == 1);

		res = q.try_pop(handle);
		CHECK_FALSE(res);
	}
}

constexpr uint32_t JobQueueMTTesterItems = 500;

template <typename TQueue>
struct JobQueueMTTester_PushPopSteal {
	using QueueType = TQueue;

	TQueue* q;
	bool* terminate;
	uint32_t thread_idx;

	std::thread t;
	uint32_t processed = 0;

	JobQueueMTTester_PushPopSteal(TQueue* q_, bool* terminate_, uint32_t idx):
			q(q_), terminate(terminate_), thread_idx(idx) {}
	JobQueueMTTester_PushPopSteal() = default;

	void init() {
		processed = 0;
		t = std::thread([this]() {
			run();
		});
	}

	void fini() {
		if (t.joinable())
			t.join();
	}

	void run() {
		mt::JobHandle handle;

		if (0 == thread_idx) {
			rnd::pseudo_random rng{};
			uint32_t itemsToInsert = JobQueueMTTesterItems;

			// This thread generates 2 items per tick and consumes one
			while (itemsToInsert > 0) {
				// Keep inserting items
				if (!q->try_push(mt::JobHandle(itemsToInsert, 0, 0))) {
					std::this_thread::yield();
					continue;
				}

				--itemsToInsert;

				if (rng.range(0, 1) == 0)
					std::this_thread::yield();

				if (!q->try_pop(handle))
					std::this_thread::yield();
				else
					++processed;
			}

			// Make sure all items are consumed
			while (!q->empty())
				std::this_thread::yield();
			*terminate = true;
		} else {
			while (!*terminate) {
				// Other threads steal work
				const bool res = q->try_steal(handle);

				// Failed race, try again
				if (!res)
					continue;

				// Empty queue, wait a bit and try again
				if (res && handle == (mt::JobHandle)mt::JobNull_t{}) {
					std::this_thread::yield();
					continue;
				}

				// Processed
				++processed;
			}
		}
	}
};

template <typename TQueue>
struct JobQueueMTTester_PushPop {
	using QueueType = TQueue;

	TQueue* q;
	bool* terminate;
	uint32_t thread_idx;

	std::thread t;
	uint32_t processed = 0;

	JobQueueMTTester_PushPop(TQueue* q_, bool* terminate_, uint32_t idx): q(q_), terminate(terminate_), thread_idx(idx) {}
	JobQueueMTTester_PushPop() = default;

	void init() {
		processed = 0;
		t = std::thread([this]() {
			run();
		});
	}

	void fini() {
		if (t.joinable())
			t.join();
	}

	void run() {
		mt::JobHandle handle;

		if (0 == thread_idx) {
			rnd::pseudo_random rng{};
			uint32_t itemsToInsert = JobQueueMTTesterItems;

			// This thread generates 2 items per tick and consumes one
			while (itemsToInsert > 0) {
				// Keep inserting items
				if (!q->try_push(mt::JobHandle(itemsToInsert, 0, 0))) {
					std::this_thread::yield();
					continue;
				}

				--itemsToInsert;

				if (rng.range(0, 1) == 0)
					std::this_thread::yield();

				if (!q->try_pop(handle))
					std::this_thread::yield();
				else
					++processed;
			}

			// Make sure all items are consumed
			while (!q->empty())
				std::this_thread::yield();
			*terminate = true;
		} else {
			while (!*terminate) {
				// Other threads steal work
				if (!q->try_pop(handle)) {
					std::this_thread::yield();
					continue;
				}

				++processed;
			}
		}
	}
};

template <typename TTestType>
void TestJobQueueMT(uint32_t threadCnt) {
	CHECK(threadCnt > 1);

	typename TTestType::QueueType q;
	bool terminate = false;
	cnt::darray<TTestType> testers;
	GAIA_FOR(threadCnt) testers.push_back(TTestType(&q, &terminate, i));

	GAIA_FOR_(100, j) {
		GAIA_FOR(threadCnt) testers[i].init();
		GAIA_FOR(threadCnt) testers[i].fini();
		uint32_t total = 0;
		GAIA_FOR(threadCnt) {
			total += testers[i].processed;
		}
		CHECK(total == JobQueueMTTesterItems);
		terminate = false;
	}
}

TEST_CASE("JobQueue") {
	using jc = mt::JobQueue<1024>;
	using mpmc = mt::MpmcQueue<mt::JobHandle, 1024>;

	SUBCASE("Basic") {
		TestJobQueue_PushPopSteal<jc>(false);
		TestJobQueue_PushPop<mpmc>(true);
	}

	using mt_tester_jc = JobQueueMTTester_PushPopSteal<jc>;
	using mt_tester_mpmc = JobQueueMTTester_PushPop<mpmc>;

	SUBCASE("MT - 2 threads") {
		TestJobQueueMT<mt_tester_jc>(2);
		TestJobQueueMT<mt_tester_mpmc>(2);
	}

	SUBCASE("MT - 4 threads") {
		TestJobQueueMT<mt_tester_jc>(4);
		TestJobQueueMT<mt_tester_mpmc>(4);
	}

	SUBCASE("MT - 16 threads") {
		TestJobQueueMT<mt_tester_jc>(16);
		TestJobQueueMT<mt_tester_mpmc>(16);
	}
}

static uint32_t JobSystemFunc(std::span<const uint32_t> arr) {
	uint32_t sum = 0;
	GAIA_EACH(arr) sum += arr[i];
	return sum;
}

template <typename Func>
void Run_Schedule_Simple(
		const uint32_t* pArr, uint32_t* pRes, uint32_t jobCnt, uint32_t ItemsPerJob, Func func, uint32_t depCnt) {
	auto& tp = mt::ThreadPool::get();
	std::atomic_uint32_t cnt = 0;

	auto* pHandles = (mt::JobHandle*)alloca(sizeof(mt::JobHandle) * jobCnt);
	GAIA_FOR(jobCnt) {
		mt::Job job;
		job.func = [&, i, func]() {
			const auto idxStart = i * ItemsPerJob;
			const auto idxEnd = (i + 1) * ItemsPerJob;
			pRes[i] += func({pArr + idxStart, idxEnd - idxStart});
			cnt.fetch_add(1, std::memory_order_relaxed);
		};
		pHandles[i] = tp.add(GAIA_MOV(job));
	}

	auto* pDepHandles = (mt::JobHandle*)alloca(sizeof(mt::JobHandle) * depCnt);
	GAIA_FOR(depCnt) {
		mt::Job dependencyJob;
		dependencyJob.func = [&]() {
			const bool isLast = cnt.load(std::memory_order_acquire) == jobCnt;
			CHECK(isLast);
		};
		pDepHandles[i] = tp.add(GAIA_MOV(dependencyJob));
		tp.dep(std::span(pHandles, jobCnt), pDepHandles[i]);
		tp.submit(pDepHandles[i]);
	}

	tp.submit(std::span(pHandles, jobCnt));

	GAIA_FOR(depCnt) {
		tp.wait(pDepHandles[i]);
	}

	GAIA_FOR(jobCnt) CHECK(pRes[i] == ItemsPerJob);
	CHECK(cnt == jobCnt);
}

TEST_CASE("Multithreading - Schedule") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t JobCount = 64;
	constexpr uint32_t ItemsPerJob = 5000;
	constexpr uint32_t N = JobCount * ItemsPerJob;

	cnt::sarray<uint32_t, JobCount> res;

	cnt::darr<uint32_t> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i] = 1;

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 1);
	}
	SUBCASE("Max workers Deps2") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 2);
	}
	SUBCASE("Max workers Deps3") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 3);
	}
	SUBCASE("Max workers Deps4") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 4);
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 1);
	}
	SUBCASE("0 workers Deps2") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 2);
	}
	SUBCASE("0 workers Deps3") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 3);
	}
	SUBCASE("0 workers Deps4") {
		tp.set_max_workers(0, 0);

		GAIA_FOR(res.max_size()) res[i] = 0;
		Run_Schedule_Simple(arr.data(), res.data(), JobCount, ItemsPerJob, JobSystemFunc, 4);
	}
}

TEST_CASE("Multithreading - Priority dependent jobs keep queue ownership") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(3, 1);

	std::atomic_bool highRanOnHighWorker = false;
	std::atomic_bool lowRanOnLowWorker = false;
	std::atomic_bool lowDone = false;

	mt::Job highJob;
	highJob.flags = mt::JobCreationFlags::ManualDelete;
	highJob.priority = mt::JobPriority::High;
	highJob.func = [&]() {
		auto* ctx = mt::detail::tl_workerCtx;
		highRanOnHighWorker.store(
				ctx != nullptr && ctx->workerIdx != 0 && ctx->prio == mt::JobPriority::High, std::memory_order_release);
	};

	mt::Job lowJob;
	lowJob.flags = mt::JobCreationFlags::ManualDelete;
	lowJob.priority = mt::JobPriority::Low;
	lowJob.func = [&]() {
		auto* ctx = mt::detail::tl_workerCtx;
		lowRanOnLowWorker.store(
				ctx != nullptr && ctx->workerIdx != 0 && ctx->prio == mt::JobPriority::Low, std::memory_order_release);
		lowDone.store(true, std::memory_order_release);
	};

	const auto highHandle = tp.add(GAIA_MOV(highJob));
	const auto lowHandle = tp.add(GAIA_MOV(lowJob));
	tp.dep(highHandle, lowHandle);
	tp.submit(lowHandle);
	tp.submit(highHandle);

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!lowDone.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
		std::this_thread::yield();

	CHECK(highRanOnHighWorker.load(std::memory_order_acquire));
	CHECK(lowDone.load(std::memory_order_acquire));
	CHECK(lowRanOnLowWorker.load(std::memory_order_acquire));

	tp.wait(lowHandle);
	tp.del(lowHandle);
	tp.del(highHandle);
}

TEST_CASE("Multithreading - Priority overflow does not inline cross-priority work") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(3, 1);

	constexpr uint32_t LowJobs = 1536;

	std::atomic_bool releaseLowJobs = false;
	std::atomic_bool lowRanOnHighWorker = false;
	std::atomic_uint32_t lowStarted = 0;

	mt::Job highJob;
	highJob.flags = mt::JobCreationFlags::ManualDelete;
	highJob.priority = mt::JobPriority::High;
	highJob.func = []() {};

	const auto highHandle = tp.add(GAIA_MOV(highJob));

	cnt::darr<mt::JobHandle> lowHandles;
	lowHandles.resize(LowJobs);

	GAIA_FOR(LowJobs) {
		mt::Job lowJob;
		lowJob.flags = mt::JobCreationFlags::ManualDelete;
		lowJob.priority = mt::JobPriority::Low;
		lowJob.func = [&]() {
			auto* ctx = mt::detail::tl_workerCtx;
			if (ctx != nullptr && ctx->workerIdx != 0 && ctx->prio == mt::JobPriority::High)
				lowRanOnHighWorker.store(true, std::memory_order_release);

			lowStarted.fetch_add(1, std::memory_order_release);
			while (!releaseLowJobs.load(std::memory_order_acquire))
				std::this_thread::yield();
		};

		lowHandles[i] = tp.add(GAIA_MOV(lowJob));
		tp.dep(highHandle, lowHandles[i]);
		tp.submit(lowHandles[i]);
	}

	tp.submit(highHandle);

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (lowStarted.load(std::memory_order_acquire) == 0 && std::chrono::steady_clock::now() < deadline)
		std::this_thread::yield();

	CHECK(lowStarted.load(std::memory_order_acquire) > 0);

	// Give the releasing worker enough time to hit the full low-priority queue path.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));

	CHECK_FALSE(lowRanOnHighWorker.load(std::memory_order_acquire));

	releaseLowJobs.store(true, std::memory_order_release);

	GAIA_EACH(lowHandles) {
		tp.wait(lowHandles[i]);
		tp.del(lowHandles[i]);
	}
	tp.del(highHandle);
}

TEST_CASE("Multithreading - Background jobs are isolated from frame update") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(2, 2);
	tp.set_background_workers(1);
	CHECK(tp.workers() == 1);
	CHECK(tp.background_workers() == 1);

	std::atomic_bool releaseBackground = false;
	std::atomic_bool backgroundStarted = false;
	std::atomic_bool backgroundDone = false;
	std::atomic_bool backgroundCtxOk = false;
	std::atomic_bool queuedBackgroundRan = false;
	std::atomic_bool queuedBackgroundCtxOk = false;
	std::atomic_bool frameDone = false;
	std::atomic_bool frameCtxOk = false;

	mt::Job backgroundJob;
	backgroundJob.flags = mt::JobCreationFlags::ManualDelete;
	backgroundJob.func = [&]() {
		auto* ctx = mt::detail::tl_workerCtx;
		backgroundCtxOk.store(ctx != nullptr && ctx->background, std::memory_order_release);
		backgroundStarted.store(true, std::memory_order_release);
		while (!releaseBackground.load(std::memory_order_acquire))
			std::this_thread::yield();
		backgroundDone.store(true, std::memory_order_release);
	};

	const auto backgroundHandle = tp.sched_background(GAIA_MOV(backgroundJob));

	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!backgroundStarted.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline)
		std::this_thread::yield();

	CHECK(backgroundStarted.load(std::memory_order_acquire));
	CHECK(backgroundCtxOk.load(std::memory_order_acquire));

	mt::Job queuedBackgroundJob;
	queuedBackgroundJob.flags = mt::JobCreationFlags::ManualDelete;
	queuedBackgroundJob.func = [&]() {
		auto* ctx = mt::detail::tl_workerCtx;
		queuedBackgroundCtxOk.store(ctx != nullptr && ctx->background, std::memory_order_release);
		queuedBackgroundRan.store(true, std::memory_order_release);
	};

	const auto queuedBackgroundHandle = tp.sched_background(GAIA_MOV(queuedBackgroundJob));

	mt::Job frameJob;
	frameJob.flags = mt::JobCreationFlags::ManualDelete;
	frameJob.func = [&]() {
		auto* ctx = mt::detail::tl_workerCtx;
		frameCtxOk.store(ctx != nullptr && !ctx->background, std::memory_order_release);
		frameDone.store(true, std::memory_order_release);
	};

	const auto frameHandle = tp.sched(GAIA_MOV(frameJob));
	tp.wait(frameHandle);
	tp.del(frameHandle);

	GAIA_FOR(64) {
		tp.update();
		std::this_thread::yield();
	}

	CHECK(frameDone.load(std::memory_order_acquire));
	CHECK(frameCtxOk.load(std::memory_order_acquire));
	CHECK_FALSE(backgroundDone.load(std::memory_order_acquire));
	CHECK_FALSE(queuedBackgroundRan.load(std::memory_order_acquire));

	releaseBackground.store(true, std::memory_order_release);
	tp.wait(backgroundHandle);
	tp.wait(queuedBackgroundHandle);

	CHECK(backgroundDone.load(std::memory_order_acquire));
	CHECK(queuedBackgroundRan.load(std::memory_order_acquire));
	CHECK(queuedBackgroundCtxOk.load(std::memory_order_acquire));

	tp.del(backgroundHandle);
	tp.del(queuedBackgroundHandle);
	tp.set_background_workers(0);
}

TEST_CASE("Multithreading - Background wait runs without background workers") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(1, 1);
	tp.set_background_workers(0);

	std::atomic_bool backgroundRan = false;

	mt::Job backgroundJob;
	backgroundJob.flags = mt::JobCreationFlags::ManualDelete;
	backgroundJob.func = [&]() {
		backgroundRan.store(true, std::memory_order_release);
	};

	const auto backgroundHandle = tp.sched_background(GAIA_MOV(backgroundJob));
	GAIA_FOR(16)
	tp.update();

	CHECK_FALSE(backgroundRan.load(std::memory_order_acquire));

	tp.wait(backgroundHandle);
	CHECK(backgroundRan.load(std::memory_order_acquire));
	tp.del(backgroundHandle);
}

TEST_CASE("Multithreading - ScheduleParallel") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t JobCount = 64;
	constexpr uint32_t ItemsPerJob = 5000;
	constexpr uint32_t N = JobCount * ItemsPerJob;

	cnt::darr<uint32_t> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i] = 1;

	std::atomic_uint32_t sum1 = 0;

	auto work = [&]() {
		mt::JobParallel j;
		j.func = [&arr, &sum1](const mt::JobArgs& args) {
			sum1 += JobSystemFunc({arr.data() + args.idxStart, args.idxEnd - args.idxStart});
		};

		auto jobHandle = tp.sched_par(GAIA_MOV(j), N, ItemsPerJob);
		tp.wait(jobHandle);
		CHECK(sum1 == N);
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

TEST_CASE("Multithreading - complete") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Jobs = 15000;

	cnt::darr<mt::JobHandle> handles;
	cnt::darr<uint32_t> res;

	handles.resize(Jobs);
	res.resize(Jobs);

	std::atomic_uint32_t cnt = 0;

	auto work = [&]() {
		GAIA_EACH(res) res[i] = BadIndex;

		GAIA_FOR(Jobs) {
			mt::Job job;
			job.flags = mt::JobCreationFlags::ManualDelete;
			job.func = [&, i]() {
				res[i] = i;
				++cnt;
			};
			handles[i] = tp.add(GAIA_MOV(job));
		}

		tp.submit(std::span(handles.data(), handles.size()));

		GAIA_FOR(Jobs) {
			tp.wait(handles[i]);
			CHECK(res[i] == i);
			tp.del(handles[i]);
		}

		CHECK(cnt == Jobs);
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

TEST_CASE("Multithreading - CompleteMany") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Iters = 15000;

	auto work = [&]() {
		srand(0);
		uint32_t res = BadIndex;

		GAIA_FOR(Iters) {
			mt::Job job0{[&res, i]() {
				res = (i + 1);
			}};
			mt::Job job1{[&res, i]() {
				res *= (i + 1);
			}};
			mt::Job job2{[&res, i]() {
				res /= (i + 1); // we add +1 everywhere to avoid division by zero at i==0
			}};

			const mt::JobHandle jobHandle[] = {tp.add(GAIA_MOV(job0)), tp.add(GAIA_MOV(job1)), tp.add(GAIA_MOV(job2))};

			tp.dep(jobHandle[0], jobHandle[1]);
			tp.dep(jobHandle[1], jobHandle[2]);

			// 2, 0, 1 -> wrong sum
			// Submit jobs in random order to make sure this doesn't work just by accident
			const uint32_t startIdx0 = (uint32_t)rand() % 3;
			const uint32_t startIdx1 = (startIdx0 + 1) % 3;
			const uint32_t startIdx2 = (startIdx0 + 2) % 3;
			tp.submit(jobHandle[startIdx0]);
			tp.submit(jobHandle[startIdx1]);
			tp.submit(jobHandle[startIdx2]);

			tp.wait(jobHandle[2]);

			CHECK(res == (i + 1));
		}
	};

	SUBCASE("Max workers") {
		const auto threads = tp.hw_thread_cnt();
		tp.set_max_workers(threads, threads);

		work();
	}
	SUBCASE("0 workers") {
		tp.set_max_workers(0, 0);

		work();
	}
}

TEST_CASE("Multithreading - Reset reusable handles") {
	auto& tp = mt::ThreadPool::get();

	constexpr uint32_t Iters = 1024;

	std::atomic_uint32_t stage = 0;
	std::atomic_uint32_t firstCnt = 0;
	std::atomic_uint32_t secondCnt = 0;
	std::atomic_bool ordered = true;

	mt::Job job1;
	job1.flags = mt::JobCreationFlags::ManualDelete;
	job1.func = [&]() {
		firstCnt.fetch_add(1, std::memory_order_relaxed);
		stage.store(1, std::memory_order_release);
	};

	mt::Job job2;
	job2.flags = mt::JobCreationFlags::ManualDelete;
	job2.func = [&]() {
		if (stage.load(std::memory_order_acquire) != 1)
			ordered.store(false, std::memory_order_relaxed);
		secondCnt.fetch_add(1, std::memory_order_relaxed);
	};

	auto handle1 = tp.add(GAIA_MOV(job1));
	auto handle2 = tp.add(GAIA_MOV(job2));
	mt::JobHandle handles[] = {handle1, handle2};

	tp.dep(handle1, handle2);
	GAIA_FOR(Iters) {
		stage.store(0, std::memory_order_relaxed);

		tp.submit(handle2);
		tp.submit(handle1);
		tp.reset(std::span(handles, 2));

		CHECK(ordered.load(std::memory_order_relaxed));
		tp.dep_refresh(handle1, handle2);
	}

	// Final run leaves handles in Done state so they can be deleted.
	stage.store(0, std::memory_order_relaxed);
	tp.submit(handle2);
	tp.submit(handle1);
	tp.wait(handle2);

	CHECK(ordered.load(std::memory_order_relaxed));
	CHECK(firstCnt.load(std::memory_order_relaxed) == Iters + 1);
	CHECK(secondCnt.load(std::memory_order_relaxed) == Iters + 1);

	tp.del(handle1);
	tp.del(handle2);
}

#if GAIA_SYSTEMS_ENABLED

struct SomeData1 {
	uint32_t val;
};
struct SomeData2 {
	uint32_t val;
};

TEST_CASE("Multithreading - Systems") {
	auto& tp = mt::ThreadPool::get();

	uint32_t data1 = 0;
	uint32_t data2 = 0;
	TestWorld twld;

	constexpr uint32_t Iters = 15000;

	{
		// 10k of SomeData1
		auto e = wld.add();
		wld.add<SomeData1>(e, {0});
		wld.copy_n(e, 9999);

		// 10k of SomeData1, SomeData2
		auto e2 = wld.add();
		wld.add<SomeData1>(e2, {0});
		wld.add<SomeData2>(e2, {0});
		wld.copy_n(e2, 9999);
	}

	auto sys1 = wld.system()
									.name("sys1")
									.all<SomeData1>()
									.all<SomeData2>() //
									.on_each([&](SomeData1, SomeData2) {
										++data1;
										++data2;
									});
	auto sys2 = wld.system()
									.name("sys2")
									.all<SomeData1>() //
									.on_each([&](SomeData1) {
										++data1;
									});

	auto handle1 = sys1.job_handle();
	auto handle2 = sys2.job_handle();

	tp.dep(handle1, handle2);
	GAIA_FOR(Iters - 1) {
		tp.submit(handle2);
		tp.submit(handle1);
		tp.wait(handle2);
		GAIA_ASSERT(data1 == 30000);
		GAIA_ASSERT(data2 == 10000);

		data1 = 0;
		data2 = 0;
		tp.reset_state(handle1);
		tp.reset_state(handle2);
		tp.dep_refresh(handle1, handle2);
	}
	{
		tp.submit(handle2);
		tp.submit(handle1);
		tp.wait(handle2);
		GAIA_ASSERT(data1 == 30000);
		GAIA_ASSERT(data2 == 10000);
	}
}

#endif

TEST_CASE("Multithreading - Reset handles missing TLS worker context") {
	auto& tp = mt::ThreadPool::get();

	// Keep this deterministic regardless of previous tests.
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	// Simulate shutdown from a thread that has no TLS worker context bound.
	mt::detail::tl_workerCtx = nullptr;

	// set_max_workers() calls reset() internally, which used to dereference null TLS context.
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);
}

TEST_CASE("Multithreading - Update auto-delete job") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(0, 0);
	CHECK(tp.workers() == 0);

	std::atomic_uint32_t executed = 0;

	// update() drives main_thread_tick() and must not double-delete auto jobs.
	constexpr uint32_t Iters = 1024;
	GAIA_FOR(Iters) {
		mt::Job job;
		job.func = [&]() {
			executed.fetch_add(1, std::memory_order_relaxed);
		};

		const auto handle = tp.add(GAIA_MOV(job));
		tp.submit(handle);
		tp.update();
	}

	CHECK(executed.load(std::memory_order_relaxed) == Iters);
}

TEST_CASE("Multithreading - Update auto-delete with workers") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	constexpr uint32_t Jobs = 2048;
	std::atomic_uint32_t executed = 0;

	cnt::darr<mt::JobHandle> handles;
	handles.resize(Jobs);

	GAIA_FOR(Jobs) {
		mt::Job job;
		job.func = [&]() {
			executed.fetch_add(1, std::memory_order_relaxed);
		};
		handles[i] = tp.add(GAIA_MOV(job));
	}

	mt::Job sync;
	sync.flags = mt::JobCreationFlags::ManualDelete;
	const auto syncHandle = tp.add(GAIA_MOV(sync));
	tp.dep(std::span(handles.data(), handles.size()), syncHandle);

	tp.submit(syncHandle);
	tp.submit(std::span(handles.data(), handles.size()));

	// Keep ticking from the main thread while worker threads are active.
	// This exercises the same path that previously double-deleted auto jobs.
	while (executed.load(std::memory_order_relaxed) < Jobs) {
		tp.update();
	}

	tp.wait(syncHandle);
	tp.del(syncHandle);
	CHECK(executed.load(std::memory_order_relaxed) == Jobs);
}

TEST_CASE("Multithreading - Handle reuse mixed delete modes") {
	auto& tp = mt::ThreadPool::get();
	tp.set_max_workers(2, 2);
	CHECK(tp.workers() == 1);

	std::atomic_uint32_t autoCnt = 0;
	std::atomic_uint32_t manualCnt = 0;

	constexpr uint32_t Iters = 4096;
	GAIA_FOR(Iters) {
		mt::Job autoJob;
		autoJob.func = [&]() {
			autoCnt.fetch_add(1, std::memory_order_relaxed);
		};

		mt::Job manualJob;
		manualJob.flags = mt::JobCreationFlags::ManualDelete;
		manualJob.func = [&]() {
			manualCnt.fetch_add(1, std::memory_order_relaxed);
		};

		const auto autoHandle = tp.add(GAIA_MOV(autoJob));
		const auto manualHandle = tp.add(GAIA_MOV(manualJob));

		tp.submit(autoHandle);
		tp.submit(manualHandle);
		tp.wait(manualHandle);
		tp.del(manualHandle);

		if ((i & 7) == 0)
			tp.update();
	}

	// Drain any remaining auto jobs.
	while (autoCnt.load(std::memory_order_relaxed) < Iters)
		tp.update();

	CHECK(autoCnt.load(std::memory_order_relaxed) == Iters);
	CHECK(manualCnt.load(std::memory_order_relaxed) == Iters);
}
