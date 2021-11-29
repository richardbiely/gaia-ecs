#pragma once

#define USE_ARRAY 1 // GAIA_USE_STL_CONTAINERS
// TODO: Implement a custom array container

#if USE_ARRAY == 1

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
