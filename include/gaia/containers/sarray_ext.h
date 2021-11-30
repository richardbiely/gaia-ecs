#pragma once

#if USE_VECTOR == 0
	#include "impl/sarray_ext.h"

namespace gaia {
	namespace containers {
		template <typename T, auto N>
		using sarray_ext = containers::sarr_ext<T, N>;
	} // namespace containers
} // namespace gaia
#else

	#error Unsupported value used for USE_ARRAY

#endif
