#include "gaia/mt/threadpool.h"
#define PICOBENCH_IMPLEMENT_WITH_MAIN
#include "gaia/external/picobench.hpp"
#include <gaia.h>

using namespace gaia;

static uint32_t BenchFunc(const uint32_t* pArr, uint32_t from, uint32_t to) {
	uint32_t sum = 0;
	for (uint32_t i = from; i < to; ++i)
		sum += pArr[i];
	return sum;
}

void BM_Schedule_Simple(picobench::state& state) {
	const auto user_data = state.user_data();
	const uint32_t N = user_data & 0xFFFFFFFF;
	const uint32_t Jobs = user_data >> 32;
	const uint32_t ItemsPerJob = N / Jobs;

	auto& tp = mt::ThreadPool::Get();

	containers::darray<uint32_t> arr;
	arr.resize(N);
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = 1;

	for (auto _: state) {
		(void)_;

		std::atomic_uint32_t sum = 0;

		for (uint32_t i = 0; i < Jobs; i++) {
			mt::Job job;
			job.func = [&arr, &sum, i, ItemsPerJob]() {
				const auto idxStart = i * ItemsPerJob;
				const auto idxEnd = (i + 1) * ItemsPerJob;
				sum += BenchFunc(arr.data(), idxStart, idxEnd);
			};
			tp.Schedule(job);
		}
		tp.CompleteAll();

		picobench::DoNotOptimize(sum);
	}
}

void BM_ScheduleParallel_Simple(picobench::state& state) {
	const auto user_data = state.user_data();
	const uint32_t N = user_data & 0xFFFFFFFF;

	auto& tp = mt::ThreadPool::Get();

	containers::darray<uint32_t> arr;
	arr.resize(N);
	for (uint32_t i = 0; i < arr.size(); ++i)
		arr[i] = 1;

	for (auto _: state) {
		(void)_;

		std::atomic_uint32_t sum = 0;

		mt::JobParallel job;
		job.func = [&arr, &sum](const mt::JobArgs& args) {
			sum += BenchFunc(arr.data(), args.idxStart, args.idxEnd);
		};

		tp.ScheduleParallel(job, N, 0);
		tp.CompleteAll();

		picobench::DoNotOptimize(sum);
	}
}

#define PICO_SETTINGS() iterations({64}).samples(3)

static constexpr uint32_t ItemsToProcess = 10'000'000;

PICOBENCH_SUITE("Schedule - Simple");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess | (1ll << 32)).label("1 thread");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess | (2ll << 32)).label("2 threads");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess | (4ll << 32)).label("4 threads");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess | (8ll << 32)).label("8 threads");
PICOBENCH(BM_Schedule_Simple)
		.PICO_SETTINGS()
		.user_data(ItemsToProcess | ((uint64_t)mt::ThreadPool::Get().GetWorkersCount()) << 32)
		.label("all workers");

PICOBENCH_SUITE("ScheduleParallel - Simple");
PICOBENCH(BM_ScheduleParallel_Simple).PICO_SETTINGS().user_data(ItemsToProcess);
