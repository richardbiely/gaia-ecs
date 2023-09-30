#define PICOBENCH_IMPLEMENT_WITH_MAIN
#include "gaia/external/picobench.hpp"
#include <gaia.h>

using namespace gaia;

static uint32_t BenchFunc_Simple(std::span<const uint32_t> arr) {
	uint32_t sum = 0;
	for (uint32_t i = 0; i < arr.size(); ++i)
		sum += arr[i];
	return sum;
}

static uint32_t BenchFunc_Complex(std::span<const uint32_t> arr) {
	uint32_t sum = 0;
	for (uint32_t i = 0; i < arr.size(); ++i)
		sum += arr[i];
	for (uint32_t i = 0; i < arr.size(); ++i)
		sum *= arr[i];
	for (uint32_t i = 0; i < arr.size(); ++i) {
		if (arr[i] != 0)
			sum %= arr[i];
	}
	for (uint32_t i = 0; i < arr.size(); ++i)
		sum *= arr[i];
	for (uint32_t i = 0; i < arr.size(); ++i) {
		if (arr[i] != 0)
			sum /= arr[i];
	}
	return sum;
}

void Run_Schedule_Empty(uint32_t Jobs) {
	auto& tp = mt::ThreadPool::Get();

	for (uint32_t i = 0; i < Jobs; i++) {
		mt::Job job;
		job.func = []() {};
		tp.Schedule(job);
	}
	tp.CompleteAll();
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
void Run_Schedule_Simple(const uint32_t* pArr, uint32_t Jobs, uint32_t ItemsPerJob, Func func) {
	auto& tp = mt::ThreadPool::Get();

	std::atomic_uint32_t sum = 0;

	for (uint32_t i = 0; i < Jobs; i++) {
		mt::Job job;
		job.func = [&pArr, &sum, i, ItemsPerJob, func]() {
			const auto idxStart = i * ItemsPerJob;
			const auto idxEnd = (i + 1) * ItemsPerJob;
			sum += func({pArr + idxStart, idxEnd - idxStart});
		};
		tp.Schedule(job);
	}
	tp.CompleteAll();

	picobench::DoNotOptimize(sum);
}

void BM_Schedule_Simple(picobench::state& state) {
	const auto user_data = state.user_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t Jobs = user_data >> 32;
	const uint32_t ItemsPerJob = N / Jobs;

	containers::darray<uint32_t> arr;
	arr.resize(N);
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = i;

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

	containers::darray<uint32_t> arr;
	arr.resize(N);
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = i;

	for (auto _: state) {
		(void)_;
		Run_Schedule_Simple(arr.data(), Jobs, ItemsPerJob, BenchFunc_Complex);
	}
}

template <typename Func>
void Run_ScheduleParallel(const uint32_t* pArr, uint32_t Items, Func func) {
	auto& tp = mt::ThreadPool::Get();

	std::atomic_uint32_t sum = 0;

	mt::JobParallel job;
	job.func = [&pArr, &sum, func](const mt::JobArgs& args) {
		sum += func({pArr + args.idxStart, args.idxEnd - args.idxStart});
	};

	tp.ScheduleParallel(job, Items, 0);
	tp.CompleteAll();

	picobench::DoNotOptimize(sum);
}

void BM_ScheduleParallel_Simple(picobench::state& state) {
	const auto user_data = state.user_data();
	const uint32_t N = user_data & 0xFFFFFFFF;

	containers::darray<uint32_t> arr;
	arr.resize(N);
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = i;

	for (auto _: state) {
		(void)_;
		Run_ScheduleParallel(arr.data(), N, BenchFunc_Simple);
	}
}

void BM_ScheduleParallel_Complex(picobench::state& state) {
	const auto user_data = state.user_data();
	const uint32_t N = user_data & 0xFFFFFFFF;

	containers::darray<uint32_t> arr;
	arr.resize(N);
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = i;

	for (auto _: state) {
		(void)_;
		Run_ScheduleParallel(arr.data(), N, BenchFunc_Complex);
	}
}

#define PICO_SETTINGS() iterations({64}).samples(3)

////////////////////////////////////////////////////////////////////////////////////////////////
// Following benchmarks should scale linearly with the number of threads added.
// If the CPU has enough threads of equal processing power available and the performance
// doesn't scale accordingly it is most likely due to scheduling overhead.
// We want to make this as small as possible.
////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////
// Measures job creation overhead.
////////////////////////////////////////////////////////////////////////////////////////////////
PICOBENCH_SUITE("Schedule - Empty");
PICOBENCH(BM_Schedule_Empty).PICO_SETTINGS().user_data(1000).label("Schedule, 1000");
PICOBENCH(BM_Schedule_Empty).PICO_SETTINGS().user_data(5000).label("Schedule, 5000");
PICOBENCH(BM_Schedule_Empty).PICO_SETTINGS().user_data(10000).label("Schedule, 10000");

////////////////////////////////////////////////////////////////////////////////////////////////
// Low load most likely to show scheduling overhead.
////////////////////////////////////////////////////////////////////////////////////////////////
static constexpr uint32_t ItemsToProcess_Trivial = 1'000;
PICOBENCH_SUITE("Schedule/ScheduleParallel - Trivial");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Trivial | (1ll << 32)).label("Schedule, 1");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Trivial | (2ll << 32)).label("Schedule, 2");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Trivial | (4ll << 32)).label("Schedule, 4");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Trivial | (8ll << 32)).label("Schedule, 8");
PICOBENCH(BM_Schedule_Simple)
		.PICO_SETTINGS()
		.user_data(ItemsToProcess_Trivial | ((uint64_t)mt::ThreadPool::Get().GetWorkersCount()) << 32)
		.label("Schedule, MAX");
PICOBENCH(BM_ScheduleParallel_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Trivial).label("ScheduleParallel");

////////////////////////////////////////////////////////////////////////////////////////////////
// Medium load. Might show scheduling overhead.
////////////////////////////////////////////////////////////////////////////////////////////////
static constexpr uint32_t ItemsToProcess_Simple = 1'000'000;
PICOBENCH_SUITE("Schedule/ScheduleParallel - Simple");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Simple | (1ll << 32)).label("Schedule, 1");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Simple | (2ll << 32)).label("Schedule, 2");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Simple | (4ll << 32)).label("Schedule, 4");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Simple | (8ll << 32)).label("Schedule, 8");
PICOBENCH(BM_Schedule_Simple)
		.PICO_SETTINGS()
		.user_data(ItemsToProcess_Simple | ((uint64_t)mt::ThreadPool::Get().GetWorkersCount()) << 32)
		.label("Schedule, MAX");
PICOBENCH(BM_ScheduleParallel_Simple).PICO_SETTINGS().user_data(ItemsToProcess_Simple).label("ScheduleParallel");

////////////////////////////////////////////////////////////////////////////////////////////////
// Bigger load. Should not be a subject to scheduling overhead.
////////////////////////////////////////////////////////////////////////////////////////////////
static constexpr uint32_t ItemsToProcess_Complex = 1'000'000;
PICOBENCH_SUITE("Schedule/ScheduleParallel - Complex");
PICOBENCH(BM_Schedule_Complex).PICO_SETTINGS().user_data(ItemsToProcess_Complex | (1ll << 32)).label("Schedule, 1");
PICOBENCH(BM_Schedule_Complex).PICO_SETTINGS().user_data(ItemsToProcess_Complex | (2ll << 32)).label("Schedule, 2");
PICOBENCH(BM_Schedule_Complex).PICO_SETTINGS().user_data(ItemsToProcess_Complex | (4ll << 32)).label("Schedule, 4");
PICOBENCH(BM_Schedule_Complex).PICO_SETTINGS().user_data(ItemsToProcess_Complex | (8ll << 32)).label("Schedule, 8");
PICOBENCH(BM_Schedule_Complex)
		.PICO_SETTINGS()
		.user_data(ItemsToProcess_Complex | ((uint64_t)mt::ThreadPool::Get().GetWorkersCount()) << 32)
		.label("Schedule, MAX");
PICOBENCH(BM_ScheduleParallel_Complex).PICO_SETTINGS().user_data(ItemsToProcess_Complex).label("ScheduleParallel");
