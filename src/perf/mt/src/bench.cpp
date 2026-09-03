#include <gaia.h>
#include <picobench/picobench.hpp>

using namespace gaia;

struct Data {
	uint32_t val;
};

static uint32_t BenchFunc_Simple(std::span<const Data> arr) {
	const auto cnt = arr.size();

	uint32_t sum = 0;
	GAIA_FOR(cnt) {
		sum += arr[i].val;
	}
	return sum;
}

static uint32_t BenchFunc_Complex(std::span<const Data> arr) {
	const auto cnt = arr.size();

	uint32_t sum = 0;
	GAIA_FOR(cnt) sum += arr[i].val;
	GAIA_FOR(cnt) sum *= arr[i].val;
	GAIA_FOR(cnt) {
		if (arr[i].val != 0)
			sum %= arr[i].val;
	}
	GAIA_FOR(cnt) sum *= arr[i].val;
	GAIA_FOR(cnt) {
		if (arr[i].val != 0)
			sum /= arr[i].val;
	}
	return sum;
}

void Run_Schedule_Empty(uint32_t Jobs) {
	auto& tp = mt::ThreadPool::get();

	mt::Job sync;
	sync.flags = mt::JobCreationFlags::ManualDelete;
	auto syncHandle = tp.add(GAIA_MOV(sync));

	auto* pHandles = static_cast<mt::JobHandle*>(alloca(sizeof(mt::JobHandle) * (Jobs + 1)));
	GAIA_FOR(Jobs) {
		mt::Job job;
		job.func = []() {};
		tp.dep(pHandles[i] = tp.add(GAIA_MOV(job)), syncHandle);
	}
	pHandles[Jobs] = syncHandle;
	tp.submit(std::span(pHandles, Jobs + 1));
	tp.wait(syncHandle);
	tp.del(syncHandle);
}

void BM_Schedule_Empty(picobench::state& state) {
	const auto user_data = state.input_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t Jobs = N;

	for (auto _: state) {
		(void)_;
		Run_Schedule_Empty(Jobs);
	}
}

template <typename Func>
void Run_Schedule_Simple(const Data* pArr, uint32_t Jobs, uint32_t ItemsPerJob, Func func) {
	auto& tp = mt::ThreadPool::get();

	mt::Job sync;
	sync.flags = mt::JobCreationFlags::ManualDelete;
	auto syncHandle = tp.add(GAIA_MOV(sync));

	std::atomic_uint32_t sum = 0;

	auto* pHandles = static_cast<mt::JobHandle*>(alloca(sizeof(mt::JobHandle) * (Jobs + 1)));
	GAIA_FOR(Jobs) {
		mt::Job job;
		job.func = [&pArr, &sum, i, ItemsPerJob, func]() {
			const auto idxStart = i * ItemsPerJob;
			const auto idxEnd = (i + 1) * ItemsPerJob;
			sum += func({pArr + idxStart, idxEnd - idxStart});
		};
		pHandles[i] = tp.add(GAIA_MOV(job));
	}
	pHandles[Jobs] = syncHandle;
	tp.dep(std::span(pHandles, Jobs), pHandles[Jobs]);
	tp.submit(std::span(pHandles, Jobs + 1));
	tp.wait(syncHandle);
	tp.del(syncHandle);

	gaia::dont_optimize(sum);
}

void BM_Schedule_Simple(picobench::state& state) {
	const auto user_data = state.input_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t Jobs = user_data >> 32;
	const uint32_t ItemsPerJob = N / Jobs;

	cnt::darray<Data> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i].val = i;

	for (auto _: state) {
		(void)_;
		Run_Schedule_Simple(arr.data(), Jobs, ItemsPerJob, BenchFunc_Simple);
	}
}

void BM_Schedule_Complex(picobench::state& state) {
	const auto user_data = state.input_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t Jobs = user_data >> 32;
	const uint32_t ItemsPerJob = N / Jobs;

	cnt::darray<Data> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i].val = i;

	for (auto _: state) {
		(void)_;
		Run_Schedule_Simple(arr.data(), Jobs, ItemsPerJob, BenchFunc_Complex);
	}
}

void BM_Schedule_ECS_Simple(picobench::state& state) {
	const auto user_data = state.input_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t ExecMode = user_data >> 32;

	ecs::World w;

	const auto type =
			ExecMode < (uint32_t)ecs::QueryExecType::ParallelEff ? (ecs::QueryExecType)ExecMode : ecs::QueryExecType::Default;

	w.system() //
			.name("BM_Schedule_ECS_Simple")
			.all<Data>()
			.mode(type)
			.on_each([&](ecs::Iter& it) {
				auto dv = it.view<Data>();
				auto sp = std::span((const Data*)dv.data(), dv.size());
				auto res = BenchFunc_Simple(sp);
				gaia::dont_optimize(res);
			});

	GAIA_FOR(N) {
		auto e = w.add();
		w.add<Data>(e, {i});
	}

	// Warm up
	w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}
}

void BM_Schedule_ECS_Complex(picobench::state& state) {
	const auto user_data = state.input_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t ExecMode = user_data >> 32;

	ecs::World w;

	const auto type =
			ExecMode < (uint32_t)ecs::QueryExecType::ParallelEff ? (ecs::QueryExecType)ExecMode : ecs::QueryExecType::Default;

	w.system() //
			.name("BM_Schedule_ECS_Complex")
			.all<Data>()
			.mode(type)
			.on_each([&](ecs::Iter& it) {
				auto dv = it.view<Data>();
				auto sp = std::span((const Data*)dv.data(), dv.size());
				auto res = BenchFunc_Complex(sp);
				gaia::dont_optimize(res);
			});

	GAIA_FOR(N) {
		auto e = w.add();
		w.add<Data>(e, {i});
	}

	// Warm up
	w.update();

	for (auto _: state) {
		(void)_;
		w.update();
	}
}

template <typename Func>
void Run_ScheduleParallel(const Data* pArr, uint32_t Items, Func func) {
	auto& tp = mt::ThreadPool::get();

	std::atomic_uint32_t sum = 0;

	mt::JobParallel job;
	job.func = [&pArr, &sum, func](const mt::JobArgs& args) {
		sum += func({pArr + args.idxStart, args.idxEnd - args.idxStart});
	};

	auto syncHandle = tp.sched_par(GAIA_MOV(job), Items, 0);
	tp.wait(syncHandle);

	gaia::dont_optimize(sum);
}

void BM_ScheduleParallel_Simple(picobench::state& state) {
	const auto user_data = state.input_data();
	const uint32_t N = user_data & 0xFFFFFFFF;

	cnt::darray<Data> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i].val = i;

	for (auto _: state) {
		(void)_;
		Run_ScheduleParallel(arr.data(), N, BenchFunc_Simple);
	}
}

void BM_ScheduleParallel_Complex(picobench::state& state) {
	const auto user_data = state.input_data();
	const uint32_t N = user_data & 0xFFFFFFFF;

	cnt::darray<Data> arr;
	arr.resize(N);
	GAIA_EACH(arr) arr[i].val = i;

	for (auto _: state) {
		(void)_;
		Run_ScheduleParallel(arr.data(), N, BenchFunc_Complex);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Main func
////////////////////////////////////////////////////////////////////////////////////////////////

#define PICO_SETTINGS() iterations({64}).samples(3)
#define PICO_SETTINGS_SANI() iterations({8}).samples(1)
#define PICOBENCH_SUITE_REG(name) r.current_suite_name() = name;
#define PICOBENCH_REG(func) (void)r.add_benchmark(#func, func)
