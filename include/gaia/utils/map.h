#pragma once

#define USE_HASHMAP GAIA_USE_STL_CONTAINERS

#if USE_HASHMAP == 1

	#include <unordered_map>

namespace gaia {
	namespace utils {
		template <typename Key, typename Data>
		using map = std::unordered_map<Key, Data>;
	}
} // namespace gaia

#elif USE_HASHMAP == 0

	#include "../external/robin_hood.h"

namespace gaia {
	namespace utils {
		template <typename Key, typename Data>
		using map = robin_hood::unordered_flat_map<Key, Data>;
	}
} // namespace gaia

#else

	#error Unsupported value used for USE_HASHMAP

#endif
