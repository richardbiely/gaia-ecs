#pragma once

#include <inttypes.h>
// TODO: Currently necessary due to std::function. Replace them!
#include <functional>

#include "gaia/core/utility.h"

#include "event.h"
#include "jobqueue.h"

namespace gaia {
	namespace mt {
		enum class JobPriority : uint8_t {
			//! High priority job. If available it should target the CPU's performance cores.
			High = 0,
			//! Low priority job. If available it should target the CPU's efficiency cores.
			Low = 1
		};
		static inline constexpr uint32_t JobPriorityCnt = 2;

		enum JobCreationFlags : uint8_t {
			Default = 0,
			//! The job is not deleted automatically. Has to be done by the used.
			ManualDelete = 0x01,
			//! The job can wait for other job (one not set as dependency).
			CanWait = 0x02
		};

		struct JobAllocCtx {
			JobPriority priority;
		};

		struct Job {
			std::function<void()> func;
			JobPriority priority = JobPriority::High;
			JobCreationFlags flags = JobCreationFlags::Default;
		};

		struct JobArgs {
			uint32_t idxStart;
			uint32_t idxEnd;
		};

		struct JobParallel {
			std::function<void(const JobArgs&)> func;
			JobPriority priority = JobPriority::High;
		};

		class ThreadPool;

		struct ThreadCtx {
			//! Thread pool pointer
			ThreadPool* tp;
			//! Worker index
			uint32_t workerIdx;
			//! Job priority
			JobPriority prio;
			//! Event signaled when a job is executed
			Event event;
			//! Lock-free work stealing queue for the jobs
			JobQueue<512> jobQueue;

			ThreadCtx() = default;
			~ThreadCtx() = default;

			void reset() {
				event.reset();
				jobQueue.clear();
			}

			ThreadCtx(const ThreadCtx& other) = delete;
			ThreadCtx& operator=(const ThreadCtx& other) = delete;
			ThreadCtx(ThreadCtx&& other) = delete;
			ThreadCtx& operator=(ThreadCtx&& other) = delete;
		};
	} // namespace mt
} // namespace gaia