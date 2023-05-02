#pragma once
#include "../config/config.h"

#define USE_SPAN GAIA_USE_STL_CONTAINERS

#if USE_SPAN && __cpp_lib_span
	#include <span>
#else
	#include "../external/span.hpp"
namespace std {
	using tcb::span;
}
#endif
