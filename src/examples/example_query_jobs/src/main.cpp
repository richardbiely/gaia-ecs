#include <cstdio>
#include <gaia.h>
#include <gaia/ecs/query_job_batch.h>

#include "thread_scheduler.h"

namespace ecs = gaia::ecs;

//! Payload modified by both blocking parallel iteration and the dependent batch.
struct Value {
	int value;
};
//! Membership added through a structural command before the dependent query matches.
struct Ready {};

//! Runs the identical consumer workload with either scheduler, checking every entity.
//! \param name Scheduler name printed with diagnostics.
//! \param scheduler Optional external scheduler. Null keeps Gaia's default scheduler.
//! \param checksum Receives the final payload sum for comparison across schedulers.
//! \return True when execution, membership, payloads and cycle rejection are correct.
bool workload(const char* name, const ecs::Sched* scheduler, int& checksum) {
	ecs::World world;
	world.add<Value>();
	world.add<Ready>(); // Register command-buffer component types before any worker runs.
	gaia::cnt::sarray<ecs::Entity, 32> entities;
	for (uint32_t i = 0; i < entities.size(); ++i) {
		entities[i] = world.add();
		world.add<Value>(entities[i], {static_cast<int>(i)});
	}

	// 1. Ordinary parallel each is blocking; no jobs or scopes to manage.
	auto values = world.query().all<Value&>();
	values.each(
			[](Value& v) {
				++v.value;
			},
			ecs::QueryExecType::Parallel);
	for (uint32_t i = 0; i < entities.size(); ++i)
		if (world.get<Value>(entities[i]).value != static_cast<int>(i) + 1)
			return false;

	// 2/3. Only the scheduler changes. Keep queries alive and stable before declaring the batch.
	if (scheduler != nullptr)
		world.set_sched(*scheduler);
	auto candidates = world.query().all<const Value>().no<Ready>();
	auto ready = world.query().all<Value&>().all<const Ready>();
	unsigned visited = 0;
	ecs::QueryJobBatch batch(world);
	const auto select = batch.add(candidates, [](ecs::Iter& it) {
		auto entities = it.view<ecs::Entity>();
		auto values = it.view<Value>();
		for (uint32_t row = 0; row < it.size(); ++row)
			if (values[row].value % 2 == 0)
				it.cmd_buffer_st().add<Ready>(entities[row]);
	});
	const auto update = batch.add(ready, [&](Value& v) {
		v.value += 10;
		++visited; // Default mode: one task, not parallel callbacks sharing this counter.
	});
	// Apply the first query's commands before matching the second query.
	if (!batch.dep(select, update))
		return false;

	for (int run = 1; run <= 2; ++run) {
		visited = 0;
		if (ready.count() != 0)
			return false;
		const auto result = batch.run();
		if (result != ecs::QueryJobBatch::Result::Completed) {
			const auto failure = batch.failure();
			std::printf(
					"%s: result=%u, query=%u, status=%u\n", name, static_cast<unsigned>(result), failure.handle.index,
					static_cast<unsigned>(failure.status));
			return false;
		}
		if (visited != 16 || ready.count() != 16 || batch.failure().handle.owner != nullptr)
			return false;
		checksum = 0;
		for (uint32_t i = 0; i < entities.size(); ++i) {
			const bool selected = i % 2 != 0;
			const int expected = static_cast<int>(i) + 1 + (selected ? 10 * run : 0);
			if (world.has<Ready>(entities[i]) != selected || world.get<Value>(entities[i]).value != expected)
				return false;
			checksum += world.get<Value>(entities[i]).value;
			// Reuse the registrations, but force fresh structural membership on the second run.
			if (run == 1 && selected)
				world.del<Ready>(entities[i]);
		}
	}

	// The same error result on both schedulers, with no partial execution.
	if (!batch.dep(update, select) || batch.run() != ecs::QueryJobBatch::Result::Cycle)
		return false;
	int afterCycle = 0;
	for (auto entity: entities)
		afterCycle += world.get<Value>(entity).value;
	if (afterCycle != checksum || visited != 16 || ready.count() != 16)
		return false;
	std::printf("%s: parallel each OK; batch Completed twice; members=16; sum=%d; Cycle rejected\n", name, checksum);
	return true;
}

//! Exercises the built-in and external schedulers and compares their verified results.
//! \return Zero on success, nonzero on any failed validation.
int main() {
	ThreadScheduler threads;
	const auto scheduler = threads.descriptor();
	int builtin = 0, external = 0;
	if (!workload("Gaia", nullptr, builtin) || !workload("std::thread", &scheduler, external) || builtin != external ||
			threads.submitted != 4 || threads.live != 0) {
		std::puts("FAIL: query jobs example");
		return 1;
	}
	std::printf("PASS: identical outcomes; external tasks=%u; live tokens=%u\n", threads.submitted, threads.live);
	return 0;
}
