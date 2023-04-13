#pragma once
#include "../config/config.h"

#define USE_HASHSET GAIA_USE_STL_CONTAINERS

#if USE_HASHSET == 1
	#include <unordered_set>
namespace gaia {
	namespace containers {
		template <typename Key>
		using set = std::unordered_set<Key>;
	} // namespace containers
} // namespace gaia
#elif USE_HASHSET == 0
	#include "../external/robin_hood.h"
namespace gaia {
	namespace containers {
		template <typename Key>
		using set = robin_hood::unordered_flat_set<Key>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_HASHSET

#endif
