#pragma once

#include "gaia/external/robin_hood.h"

namespace gaia {
	namespace cnt {
		//! Flat hash set used by Gaia-ECS containers.
		//! \tparam Key Key type.
		template <typename Key>
		using set = robin_hood::unordered_flat_set<Key>;
	} // namespace cnt
} // namespace gaia
