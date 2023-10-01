#pragma once

#include "../external/robin_hood.h"

namespace gaia {
	namespace containers {
		template <typename Key>
		using set = robin_hood::unordered_flat_set<Key>;
	} // namespace containers
} // namespace gaia
