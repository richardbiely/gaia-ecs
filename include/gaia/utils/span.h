#pragma once
#include "../config/config.h"

#if 0 // GAIA_USE_STD_SPAN
	#include <span>
#else
	#include "../external/span.hpp"
namespace std {
	using tcb::span;
}
#endif
