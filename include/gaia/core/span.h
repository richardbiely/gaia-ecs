#pragma once
#include "../config/config.h"

#if GAIA_USE_STD_SPAN
	#include <span>
#else
	#include "impl/span_impl.h"
namespace std {
	using gaia::core::span;
}
#endif
