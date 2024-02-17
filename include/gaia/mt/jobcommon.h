#pragma once

#include <functional>
#include <inttypes.h>

namespace gaia {
	namespace mt {
		enum JobPriority : uint32_t {
			//! High priority job. If avaialble it should target the CPU's performance cores
			High = 0,
			//! Low priority job. If available it should target the CPU's efficiency cores
			Low = 1,
			//! Don't use
			Cnt = 2
		};

		struct JobAllocCtx {
			JobPriority priority;
		};

		struct Job {
			std::function<void()> func;
			JobPriority priority = JobPriority::High;
		};

		struct JobArgs {
			uint32_t idxStart;
			uint32_t idxEnd;
		};

		struct JobParallel {
			std::function<void(const JobArgs&)> func;
			JobPriority priority = JobPriority::High;
		};
	} // namespace mt
} // namespace gaia