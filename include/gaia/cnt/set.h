#pragma once

#include "gaia/external/robin_hood.h"

namespace gaia {
	namespace cnt {
		template <typename Key>
		using set = robin_hood::unordered_flat_set<Key>;
	} // namespace cnt
} // namespace gaia
