#define PICOBENCH_IMPLEMENT
#include <gaia.h>
#include <picobench/picobench.hpp>

using namespace gaia;

struct Data {
	uint32_t val;
};

static uint32_t BenchFunc_Simple(std::span<const Data> arr) {
	uint32_t sum = 0;
	GAIA_EACH(arr) sum += arr[i].val;
	return sum;
}

static uint32_t BenchFunc_Complex(std::span<const Data> arr) {
	uint32_t sum = 0;
	GAIA_EACH(arr) sum += arr[i].val;
	GAIA_EACH(arr) sum *= arr[i].val;
	GAIA_EACH(arr) {
		if (arr[i].val != 0)
			sum %= arr[i].val;
	}
	GAIA_EACH(arr) sum *= arr[i].val;
	GAIA_EACH(arr) {
		if (arr[i].val != 0)
			sum /= arr[i].val;
	}
	return sum;
}

void Run_Schedule_Empty(uint32_t Jobs) {
	auto& tp = mt::ThreadPool::get();

	mt::Job sync;
	auto syncHandle = tp.add(sync);

	auto* pHandles = (mt::JobHandle*)alloca(sizeof(mt::JobHandle) * (Jobs + 1));
	GAIA_FOR(Jobs) {
		mt::Job job;
		job.func = []() {};
		tp.dep(pHandles[i] = tp.add(job), syncHandle);
	}
	pHandles[Jobs] = syncHandle;
	tp.submit(std::span(pHandles, Jobs + 1));
	tp.wait(syncHandle);
	tp.del(std::span(pHandles, Jobs + 1));
}

void BM_Schedule_Empty(picobench::state& state) {
	const auto user_data = state.user_data();
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
	auto syncHandle = tp.add(sync);

	std::atomic_uint32_t sum = 0;

	auto* pHandles = (mt::JobHandle*)alloca(sizeof(mt::JobHandle) * (Jobs + 1));
	GAIA_FOR(Jobs) {
		mt::Job job;
		job.func = [&pArr, &sum, i, ItemsPerJob, func]() {
			const auto idxStart = i * ItemsPerJob;
			const auto idxEnd = (i + 1) * ItemsPerJob;
			sum += func({pArr + idxStart, idxEnd - idxStart});
		};
		pHandles[i] = tp.add(job);
	}
	pHandles[Jobs] = syncHandle;
	tp.dep(std::span(pHandles, Jobs), pHandles[Jobs]);
	tp.submit(std::span(pHandles, Jobs + 1));
	tp.wait(syncHandle);
	tp.del(std::span(pHandles, Jobs + 1));

	gaia::dont_optimize(sum);
}

void BM_Schedule_Simple(picobench::state& state) {
	const auto user_data = state.user_data();
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
	const auto user_data = state.user_data();
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
	const auto user_data = state.user_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t ExecMode = user_data >> 32;

	ecs::World w;

	const auto type =
			ExecMode < (uint32_t)ecs::QueryExecType::ParallelEff ? (ecs::QueryExecType)ExecMode : ecs::QueryExecType::Default;

	auto sb = w.system() //
								.all<Data>()
								.mode(type)
								.on_each([&](ecs::Iter& it) {
									auto dv = it.view<Data>();
									auto sp = std::span((const Data*)dv.data(), dv.size());
									auto res = BenchFunc_Simple(sp);
									gaia::dont_optimize(res);
								});
	w.name(sb.entity(), "BM_Schedule_ECS_Simple");

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
	const auto user_data = state.user_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t ExecMode = user_data >> 32;

	ecs::World w;

	const auto type =
			ExecMode < (uint32_t)ecs::QueryExecType::ParallelEff ? (ecs::QueryExecType)ExecMode : ecs::QueryExecType::Default;

	auto sb = w.system() //
								.all<Data>()
								.mode(type)
								.on_each([&](ecs::Iter& it) {
									auto dv = it.view<Data>();
									auto sp = std::span((const Data*)dv.data(), dv.size());
									auto res = BenchFunc_Complex(sp);
									gaia::dont_optimize(res);
								});
	w.name(sb.entity(), "BM_Schedule_ECS_Complex");

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

	auto syncHandle = tp.sched_par(job, Items, 0);
	tp.wait(syncHandle);

	gaia::dont_optimize(sum);
}

void BM_ScheduleParallel_Simple(picobench::state& state) {
	const auto user_data = state.user_data();
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
	const auto user_data = state.user_data();
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

int main(int argc, char* argv[]) {
	picobench::runner r(true);
	r.parse_cmd_line(argc, argv);

	// If picobench encounters an unknown command line argument it returns false and sets an error.
	// Ignore this behavior.
	// We only need to make sure to provide the custom arguments after the picobench ones.
	if (r.error() == picobench::error_unknown_cmd_line_argument)
		r.set_error(picobench::no_error);

	// With profiling mode enabled we want to be able to pick what benchmark to run so it is easier
	// for us to isolate the results.
	{
		bool profilingMode = false;
		bool sanitizerMode = false;

		const gaia::cnt::darray<std::string_view> args(argv + 1, argv + argc);
		for (const auto& arg: args) {
			if (arg == "-p") {
				profilingMode = true;
				continue;
			}
			if (arg == "-s") {
				sanitizerMode = true;
				continue;
			}
		}

		GAIA_LOG_N("Profiling mode = %s", profilingMode ? "ON" : "OFF");
		GAIA_LOG_N("Sanitizer mode = %s", sanitizerMode ? "ON" : "OFF");

		static constexpr uint32_t ItemsToProcess_Trivial = 1'000;
		static constexpr uint32_t ItemsToProcess_Simple = 1'000'000;
		static constexpr uint32_t ItemsToProcess_Complex = 1'000'000;

		if (profilingMode) {
			PICOBENCH_SUITE("ECS");
			PICOBENCH_REG(BM_Schedule_ECS_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("Par");
			PICOBENCH_REG(BM_Schedule_ECS_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("Par");
			r.run_benchmarks();
			return 0;
		} else if (sanitizerMode) {
			PICOBENCH_SUITE("ECS");
			PICOBENCH_REG(BM_Schedule_ECS_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("Par");
			PICOBENCH_REG(BM_Schedule_ECS_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("Par");
			r.run_benchmarks();
			return 0;
		} else {
			////////////////////////////////////////////////////////////////////////////////////////////////
			// Following benchmarks should scale linearly with the number of threads added.
			// If the CPU has enough threads of equal processing power available and the performance
			// doesn't scale accordingly it is most likely due to scheduling overhead.
			// We want to make this as small as possible.
			////////////////////////////////////////////////////////////////////////////////////////////////

			const auto workersCnt = mt::ThreadPool::get().workers();

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Measures job creation overhead.
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE("Schedule - Empty");
			PICOBENCH_REG(BM_Schedule_Empty).PICO_SETTINGS().user_data(1000).label("Schedule, 1000");
			PICOBENCH_REG(BM_Schedule_Empty).PICO_SETTINGS().user_data(5000).label("Schedule, 5000");
			PICOBENCH_REG(BM_Schedule_Empty).PICO_SETTINGS().user_data(10000).label("Schedule, 10000");

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Low load most likely to show scheduling overhead.
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE("Schedule/ScheduleParallel - Trivial");
			PICOBENCH_REG(BM_Schedule_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | (1ll << 32))
					.label("Schedule, 1");
			PICOBENCH_REG(BM_Schedule_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | (2ll << 32))
					.label("Schedule, 2");
			PICOBENCH_REG(BM_Schedule_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | (4ll << 32))
					.label("Schedule, 4");
			PICOBENCH_REG(BM_Schedule_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | (8ll << 32))
					.label("Schedule, 8");
			if (workersCnt > 8) {
				PICOBENCH_REG(BM_Schedule_Simple)
						.PICO_SETTINGS()
						.user_data(ItemsToProcess_Trivial | ((uint64_t)workersCnt) << 32)
						.label("Schedule, MAX");
			}
			PICOBENCH_REG(BM_ScheduleParallel_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial)
					.label("ScheduleParallel");

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Medium load. Might show scheduling overhead.
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE("Schedule/ScheduleParallel - Simple");
			PICOBENCH_REG(BM_Schedule_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | (1ll << 32))
					.label("Schedule, 1");
			PICOBENCH_REG(BM_Schedule_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | (2ll << 32))
					.label("Schedule, 2");
			PICOBENCH_REG(BM_Schedule_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | (4ll << 32))
					.label("Schedule, 4");
			PICOBENCH_REG(BM_Schedule_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | (8ll << 32))
					.label("Schedule, 8");
			if (workersCnt > 8) {
				PICOBENCH_REG(BM_Schedule_Simple)
						.PICO_SETTINGS()
						.user_data(ItemsToProcess_Simple | ((uint64_t)workersCnt) << 32)
						.label("Schedule, MAX");
			}
			PICOBENCH_REG(BM_ScheduleParallel_Simple)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple)
					.label("ScheduleParallel");

			////////////////////////////////////////////////////////////////////////////////////////////////
			// Bigger load. Should not be a subject to scheduling overhead.
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE("Schedule/ScheduleParallel - Complex");
			PICOBENCH_REG(BM_Schedule_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | (1ll << 32))
					.label("Schedule, 1");
			PICOBENCH_REG(BM_Schedule_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | (2ll << 32))
					.label("Schedule, 2");
			PICOBENCH_REG(BM_Schedule_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | (4ll << 32))
					.label("Schedule, 4");
			PICOBENCH_REG(BM_Schedule_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | (8ll << 32))
					.label("Schedule, 8");
			if (workersCnt > 8) {
				PICOBENCH_REG(BM_Schedule_Complex)
						.PICO_SETTINGS()
						.user_data(ItemsToProcess_Complex | ((uint64_t)workersCnt) << 32)
						.label("Schedule, MAX");
			}
			PICOBENCH_REG(BM_ScheduleParallel_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex)
					.label("ScheduleParallel");

			////////////////////////////////////////////////////////////////////////////////////////////////
			// ECS
			////////////////////////////////////////////////////////////////////////////////////////////////
			PICOBENCH_SUITE("ECS");
			PICOBENCH_REG(BM_Schedule_ECS_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("Par, 1T");
			PICOBENCH_REG(BM_Schedule_ECS_Simple) //
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Simple | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("Par, 1M");
			PICOBENCH_REG(BM_Schedule_ECS_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Trivial | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("Par, 1T");
			PICOBENCH_REG(BM_Schedule_ECS_Complex)
					.PICO_SETTINGS()
					.user_data(ItemsToProcess_Complex | ((uint64_t)ecs::QueryExecType::Parallel << 32))
					.label("Par, 1M");
		}
	}

	return r.run(0);
}