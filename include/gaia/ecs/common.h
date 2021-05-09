#pragma once
#include <inttypes.h>

namespace gaia {
	namespace ecs {
		//! Size of one chunk
		constexpr uint32_t CHUNK_SIZE = 16384;
		//! Maximum number of components on archetype
		constexpr uint32_t MAX_COMPONENTS_PER_ARCHETYPE = 32u;

		constexpr bool VerifyMaxComponentCountPerArchetype(const uint32_t count) {
			return count <= MAX_COMPONENTS_PER_ARCHETYPE;
		}

		bool DidVersionChange(uint32_t changeVersion, uint32_t requiredVersion) {
			// When a system runs for the first time, everything is considered
			// changed.
			if (requiredVersion == 0)
				return true;

			// Supporting wrap around for version numbers, change must be bigger than
			// last system run (never detect change of something the system itself
			// changed).
			return (int)(changeVersion - requiredVersion) > 0;
		}

		void UpdateVersion(uint32_t& version) {
			++version;
			// Handle wrap around, 0 is reserved for systems that have never run..
			if (version == 0)
				++version;
		}
	} // namespace ecs
} // namespace gaia
