#pragma once

#define USE_ARRAY 0

#if USE_ARRAY == 0

	#include <array>

namespace gaia {
	namespace utils {
		template <typename T, size_t N>
		using array = std::array<T, N>;
	}
} // namespace gaia

#else

	#error Unsupported value used for USE_ARRAY

#endif
