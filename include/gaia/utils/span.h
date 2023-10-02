#pragma once
#include "../config/config.h"

#if GAIA_USE_STD_SPAN
	#include <span>
#else
	#include "../external/span.hpp"
namespace std {
	using TCB_SPAN_NAMESPACE_NAME::span;
}
#endif
