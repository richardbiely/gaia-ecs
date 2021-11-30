#pragma once

#define USE_ARRAY 1 // GAIA_USE_STL_CONTAINERS

#if USE_ARRAY == 1
	#include <array>
namespace gaia {
	namespace containers {
		template <typename T, auto N>
		using sarray = std::array<T, N>;
	} // namespace containers
} // namespace gaia
#elif USE_VECTOR == 0
	#include "impl/sarray.h"
namespace gaia {
	namespace containers {
		template <typename T, auto N>
		using sarray = containers::sarr<T, N>;
	} // namespace containers
} // namespace gaia
#else

	#error Unsupported value used for USE_ARRAY

#endif
