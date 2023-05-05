#pragma once
#include "../config/config_core.h"
#include <cinttypes>

namespace gaia {
	namespace ecs {
		//! Number of ticks before empty chunks are removed
		constexpr uint16_t MAX_CHUNK_LIFESPAN = 8U;
		//! Number of ticks before empty archetypes are removed
		// constexpr uint32_t MAX_ARCHETYPE_LIFESPAN = 8U; Keep commented until used to avoid compilation errors

		GAIA_NODISCARD inline bool DidVersionChange(uint32_t changeVersion, uint32_t requiredVersion) {
			// When a system runs for the first time, everything is considered changed.
			if GAIA_UNLIKELY (requiredVersion == 0U)
				return true;

			// Supporting wrap-around for version numbers. ChangeVersion must be
			// bigger than requiredVersion (never detect change of something the
			// system itself changed).
			return (int)(changeVersion - requiredVersion) > 0;
		}

		inline void UpdateVersion(uint32_t& version) {
			++version;
			// Handle wrap-around, 0 is reserved for systems that have never run.
			if GAIA_UNLIKELY (version == 0U)
				++version;
		}
	} // namespace ecs
} // namespace gaia
