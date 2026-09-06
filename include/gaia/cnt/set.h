#pragma once

#include "gaia/config/config_core.h"

GAIA_CLANG_WARNING_PUSH()
GAIA_GCC_WARNING_PUSH()
GAIA_CLANG_WARNING_DISABLE("-Wpadded")
GAIA_CLANG_WARNING_DISABLE("-Wvariadic-macro-arguments-omitted")
GAIA_CLANG_WARNING_DISABLE("-Wsign-conversion")
GAIA_CLANG_WARNING_DISABLE("-Wconversion")
GAIA_GCC_WARNING_DISABLE("-Wpadded")
GAIA_GCC_WARNING_DISABLE("-Wsign-conversion")
GAIA_GCC_WARNING_DISABLE("-Wconversion")
#include "gaia/external/robin_hood.h"
GAIA_GCC_WARNING_POP()
GAIA_CLANG_WARNING_POP()

namespace gaia {
	namespace cnt {
		//! Flat hash set used by Gaia-ECS containers.
		//! \tparam Key Key type.
		template <typename Key>
		using set = robin_hood::unordered_flat_set<Key>;
	} // namespace cnt
} // namespace gaia
