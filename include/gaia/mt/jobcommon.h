#pragma once

#include <functional>
#include <inttypes.h>

#include "../config/config_core.h"

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

		struct JobHandle {
			uint32_t idx;

			GAIA_NODISCARD bool operator==(JobHandle other) const {
				return idx == other.idx;
			}
			GAIA_NODISCARD bool operator!=(JobHandle other) const {
				return idx != other.idx;
			}
		};

		static constexpr JobHandle JobHandleInvalid = JobHandle{(uint32_t)-1};
	} // namespace mt
} // namespace gaia