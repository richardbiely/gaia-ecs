#pragma once

// TODO: There is no quickly achievable std alternative so go with gaia container
#if 1// USE_VECTOR == 0
	#include "impl/sarray_ext_impl.h"

namespace gaia {
	namespace containers {
		template <typename T, auto N>
		using sarray_ext = containers::sarr_ext<T, N>;
	} // namespace containers
} // namespace gaia
#else

	// You can add your custom container here
	#error Unsupported value used for USE_ARRAY

#endif
