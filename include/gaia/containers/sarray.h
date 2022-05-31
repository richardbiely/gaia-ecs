#pragma once

#define USE_ARRAY GAIA_USE_STL_CONTAINERS

#if USE_ARRAY == 1
	#include <array>
namespace gaia {
	namespace containers {
		template <typename T, size_t N>
		using sarray = std::array<T, N>;
	} // namespace containers
} // namespace gaia
#elif USE_VECTOR == 0
	#include "impl/sarray_impl.h"
namespace gaia {
	namespace containers {
		template <typename T, size_t N>
		using sarray = containers::sarr<T, N>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_ARRAY

#endif
