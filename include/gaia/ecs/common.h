#pragma once
#include "gaia/config/config.h"

#include <cstdint>

namespace gaia {
	namespace ecs {
		//! Tests whether a change version is newer than a required version, including wraparound.
		//! \param changeVersion Version recorded when data last changed.
		//! \param requiredVersion Baseline version required by the caller.
		//! \return True when the change occurred after the baseline.
		GAIA_NODISCARD inline bool version_changed(uint32_t changeVersion, uint32_t requiredVersion) {
			// When a system runs for the first time, everything is considered changed.
			if GAIA_UNLIKELY (requiredVersion == 0U)
				return true;

			// Supporting wrap-around for version numbers. ChangeVersion must be
			// bigger than requiredVersion (never detect change of something the
			// system itself changed).
			return (int)(changeVersion - requiredVersion) > 0;
		}

		//! Advances a version counter while reserving zero as an invalid value.
		//! \param version Version counter to advance.
		inline void update_version(uint32_t& version) {
			++version;
			// Handle wrap-around, 0 is reserved for systems that have never run.
			if GAIA_UNLIKELY (version == 0U)
				++version;
		}
	} // namespace ecs
} // namespace gaia
