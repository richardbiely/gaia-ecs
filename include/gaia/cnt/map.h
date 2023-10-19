#pragma once

#include "../external/robin_hood.h"

namespace gaia {
	namespace cnt {
		template <typename Key, typename Data>
		using map = robin_hood::unordered_flat_map<Key, Data>;
	} // namespace cnt
} // namespace gaia
