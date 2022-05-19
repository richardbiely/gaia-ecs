#pragma once

#if __cpp_lib_span
	#include <span>
#else
	// Workaround for pre-C++20 compilers <span>
	#include "../external/span.hpp"
namespace std {
	using tcb::span;
}
#endif
