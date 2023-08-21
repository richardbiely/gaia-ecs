#include "gaia/mt/threadpool.h"
#define PICOBENCH_IMPLEMENT_WITH_MAIN
#include "gaia/external/picobench.hpp"
#include <gaia.h>

using namespace gaia;

static uint32_t BenchFunc(std::span<uint32_t> arr) {
	uint32_t sum = 0;
	for (uint32_t i = 0; i < arr.size(); ++i)
		sum += arr[i];
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
				sum += BenchFunc(std::span(arr.data() + idxStart, idxEnd - idxStart));
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
			sum += BenchFunc(std::span(arr.data() + args.idxStart, args.idxEnd - args.idxStart));
		};

		tp.ScheduleParallel(job, N, 0);
		tp.CompleteAll();

		picobench::DoNotOptimize(sum);
	}
}

#define PICO_SETTINGS() iterations({64}).samples(3)

static constexpr uint32_t ItemsToProcess = 10'000'000;

PICOBENCH_SUITE("Schedule/ScheduleParallel - Simple");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess | (1ll << 32)).label("Schedule, 1T");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess | (2ll << 32)).label("Schedule, 2T");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess | (4ll << 32)).label("Schedule, 4T");
PICOBENCH(BM_Schedule_Simple).PICO_SETTINGS().user_data(ItemsToProcess | (8ll << 32)).label("Schedule, 8T");
PICOBENCH(BM_Schedule_Simple)
		.PICO_SETTINGS()
		.user_data(ItemsToProcess | ((uint64_t)mt::ThreadPool::Get().GetWorkersCount()) << 32)
		.label("Schedule, all workers");
PICOBENCH(BM_ScheduleParallel_Simple).PICO_SETTINGS().user_data(ItemsToProcess).label("ScheduleParallel");
