#pragma once

#define USE_VECTOR 0

#if USE_VECTOR == 0

	#include <vector>

namespace gaia {
	namespace utils {
		template <typename T>
		using vector = std::vector<T>;
	}
} // namespace gaia

#else

	#error Unsupported value used for USE_VECTOR

#endif
