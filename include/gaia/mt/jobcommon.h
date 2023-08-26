#pragma once

#include <functional>
#include <inttypes.h>

namespace gaia {
	namespace mt {
		struct Job {
			std::function<void()> func;
		};

		struct JobArgs {
			uint32_t idxStart;
			uint32_t idxEnd;
		};

		struct JobParallel {
			std::function<void(const JobArgs&)> func;
		};
	} // namespace mt
} // namespace gaia