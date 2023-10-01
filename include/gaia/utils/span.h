#pragma once
#include "../config/config.h"

#define GAIA_USE_STD_SPAN (GAIA_USE_STL_CONTAINERS && GAIA_CPP_VERSION(202002L) && __has_include(<span>))

#if GAIA_USE_STD_SPAN
	#include <span>
#else
	#include "../external/span.hpp"
namespace std {
	using tcb::span;
}
#endif
