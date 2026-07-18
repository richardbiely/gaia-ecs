#pragma once

#include "gaia/external/robin_hood.h"

namespace gaia {
	namespace cnt {
		//! Flat hash map used by Gaia-ECS containers.
		//! \tparam Key Key type.
		//! \tparam Data Mapped value type.
		template <typename Key, typename Data>
		using map = robin_hood::unordered_flat_map<Key, Data>;
	} // namespace cnt
} // namespace gaia
