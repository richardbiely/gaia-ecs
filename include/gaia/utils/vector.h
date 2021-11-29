#pragma once

#define USE_VECTOR GAIA_USE_STL_CONTAINERS

#if USE_VECTOR == 1

	#include <vector>

namespace gaia {
	namespace utils {
		template <typename T>
		using darray = std::vector<T>;
	}
} // namespace gaia

#elif USE_VECTOR == 0

	#include "containers/vector.h"

namespace gaia {
	namespace utils {
		template <typename T>
		using darray = gaia::utils::vector<T>;
	}
} // namespace gaia
#else

	#error Unsupported value used for USE_VECTOR

#endif
