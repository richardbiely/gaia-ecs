#pragma once

#if __has_include(<span>)
	#include <span>
#else
	// Workaround for pre-C++20 compilers <span>
	#include "../external/span.hpp"
namespace std {
	using tcb::span;
}
#endif