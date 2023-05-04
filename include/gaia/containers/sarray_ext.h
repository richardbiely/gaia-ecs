#pragma once

// TODO: There is no quickly achievable std alternative so go with gaia container
#include "impl/sarray_ext_impl.h"

namespace gaia {
	namespace containers {
		template <typename T, auto N>
		using sarray_ext = containers::sarr_ext<T, N>;
	} // namespace containers
} // namespace gaia
