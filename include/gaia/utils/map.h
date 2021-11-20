#pragma once

#define USE_HASHMAP 0

#if USE_HASHMAP == 0

	#include <unordered_map>

namespace gaia {
	namespace utils {
		template <typename Key, typename Data>
		using map = std::unordered_map<Key, Data>;
	}
} // namespace gaia

#elif USE_HASHMAP == 1

	#include "../external/robin_hood.h"

namespace gaia {
	namespace utils {
		template <typename Key, typename Data>
		using map = robin_hood::unordered_map<Key, Data>;
	}
} // namespace gaia

#endif
