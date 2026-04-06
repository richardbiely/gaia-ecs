#include "common.h"
#include "registry.h"

namespace {
	constexpr uint32_t PingPongOps = 64 * 1024;
	constexpr uint32_t BatchSize = 4096;
	constexpr uint32_t BatchRounds = 16;

	template <typename TPrepare, typename TAlloc, typename TFree>
	void run_ping_pong(picobench::state& state, TPrepare&& prepare, TAlloc&& allocFn, TFree&& freeFn) {
		const auto bytes = (uint32_t)state.user_data();

		for (auto _: state) {
			(void)_;

			state.stop_timer();
			prepare(bytes);
			state.start_timer();

			uintptr_t sum = 0;
			GAIA_FOR(PingPongOps) {
				void* p = allocFn(bytes);
				((uint8_t*)p)[0] = (uint8_t)i;
				sum += (uintptr_t)p >> 4;
				freeFn(p);
			}

			gaia::dont_optimize(sum);
			state.stop_timer();
		}
	}

	template <typename TPrepare, typename TAlloc, typename TFree>
	void run_batch(picobench::state& state, TPrepare&& prepare, TAlloc&& allocFn, TFree&& freeFn) {
		const auto bytes = (uint32_t)state.user_data();
		cnt::darray<void*> ptrs;
		ptrs.resize(BatchSize);

		for (auto _: state) {
			(void)_;

			state.stop_timer();
			prepare(bytes);
			state.start_timer();

			uintptr_t sum = 0;
			for (uint32_t round = 0; round < BatchRounds; ++round) {
				for (uint32_t idx = 0; idx < BatchSize; ++idx) {
					void* p = allocFn(bytes);
					ptrs[idx] = p;
					((uint8_t*)p)[0] = (uint8_t)(idx + round);
					sum += (uintptr_t)p >> 5;
				}

				for (uint32_t idx = 0; idx < BatchSize; ++idx)
					freeFn(ptrs[idx]);
			}

			gaia::dont_optimize(sum);
			state.stop_timer();
		}
	}

	void prepare_smallblock(uint32_t bytes) {
		auto& alloc = mem::SmallBlockAllocator::get();
		alloc.flush(true);

		// Warm one size class outside the timed section so the benchmark reports steady-state reuse.
		cnt::darray<void*> ptrs;
		ptrs.resize(BatchSize);

		for (uint32_t i = 0; i < BatchSize; ++i)
			ptrs[i] = alloc.alloc(bytes);
		for (uint32_t i = 0; i < BatchSize; ++i)
			alloc.free(ptrs[i]);
	}

	void prepare_default([[maybe_unused]] uint32_t bytes) {}
} // namespace

void BM_DefaultAllocator_PingPong(picobench::state& state) {
	run_ping_pong(
			state, prepare_default,
			[](uint32_t bytes) {
				return mem::mem_alloc(bytes);
			},
			[](void* p) {
				mem::mem_free(p);
			});
}

void BM_SmallBlockAllocator_PingPong(picobench::state& state) {
	run_ping_pong(
			state, prepare_smallblock,
			[](uint32_t bytes) {
				return mem::SmallBlockAllocator::get().alloc(bytes);
			},
			[](void* p) {
				mem::SmallBlockAllocator::get().free(p);
			});
}

void BM_DefaultAllocator_Batch(picobench::state& state) {
	run_batch(
			state, prepare_default,
			[](uint32_t bytes) {
				return mem::mem_alloc(bytes);
			},
			[](void* p) {
				mem::mem_free(p);
			});
}

void BM_SmallBlockAllocator_Batch(picobench::state& state) {
	run_batch(
			state, prepare_smallblock,
			[](uint32_t bytes) {
				return mem::SmallBlockAllocator::get().alloc(bytes);
			},
			[](void* p) {
				mem::SmallBlockAllocator::get().free(p);
			});
}

////////////////////////////////////////////////////////////////////////////////

void register_allocators(PerfRunMode mode) {
	switch (mode) {
		case PerfRunMode::Profiling:
			PICOBENCH_SUITE_REG("Profile picks");
			PICOBENCH_REG(BM_DefaultAllocator_Batch).PICO_SETTINGS_FOCUS().user_data(128).label("default alloc batch 128");
			PICOBENCH_REG(BM_SmallBlockAllocator_Batch)
					.PICO_SETTINGS_FOCUS()
					.user_data(128)
					.label("smallblock alloc batch 128");
			return;
		case PerfRunMode::Sanitizer:
			PICOBENCH_SUITE_REG("Sanitizer picks");
			PICOBENCH_REG(BM_DefaultAllocator_PingPong)
					.PICO_SETTINGS_SANI()
					.user_data(128)
					.label("default alloc pingpong 128");
			PICOBENCH_REG(BM_SmallBlockAllocator_PingPong)
					.PICO_SETTINGS_SANI()
					.user_data(128)
					.label("smallblock alloc pingpong 128");
			return;
		case PerfRunMode::Normal:
			PICOBENCH_SUITE_REG("Allocators ping-pong");
			PICOBENCH_REG(BM_DefaultAllocator_PingPong).PICO_SETTINGS().user_data(32).label("default alloc 32");
			PICOBENCH_REG(BM_SmallBlockAllocator_PingPong).PICO_SETTINGS().user_data(32).label("smallblock alloc 32");
			PICOBENCH_REG(BM_DefaultAllocator_PingPong).PICO_SETTINGS().user_data(128).label("default alloc 128");
			PICOBENCH_REG(BM_SmallBlockAllocator_PingPong).PICO_SETTINGS().user_data(128).label("smallblock alloc 128");
			PICOBENCH_REG(BM_DefaultAllocator_PingPong).PICO_SETTINGS().user_data(512).label("default alloc 512");
			PICOBENCH_REG(BM_SmallBlockAllocator_PingPong).PICO_SETTINGS().user_data(512).label("smallblock alloc 512");

			PICOBENCH_SUITE_REG("Allocators batch");
			PICOBENCH_REG(BM_DefaultAllocator_Batch).PICO_SETTINGS().user_data(32).label("default alloc 32");
			PICOBENCH_REG(BM_SmallBlockAllocator_Batch).PICO_SETTINGS().user_data(32).label("smallblock alloc 32");
			PICOBENCH_REG(BM_DefaultAllocator_Batch).PICO_SETTINGS().user_data(128).label("default alloc 128");
			PICOBENCH_REG(BM_SmallBlockAllocator_Batch).PICO_SETTINGS().user_data(128).label("smallblock alloc 128");
			PICOBENCH_REG(BM_DefaultAllocator_Batch).PICO_SETTINGS().user_data(512).label("default alloc 512");
			PICOBENCH_REG(BM_SmallBlockAllocator_Batch).PICO_SETTINGS().user_data(512).label("smallblock alloc 512");
			return;
	}
}
