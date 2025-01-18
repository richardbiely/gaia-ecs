#pragma once
#include "../config/config.h"

// The same gaia headers used inside span_impl.h must be included here.
// Amalgamated file would not be generated properly otherwise
// because of the conditional nature of usage of this file's usage.
#include "iterator.h"
#include "utility.h"

#if GAIA_USE_STD_SPAN
	#include <span>
#else
	#include "impl/span_impl.h"
namespace std {
	using gaia::core::span;
}
#endif
