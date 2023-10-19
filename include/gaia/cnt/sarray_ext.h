#pragma once

// TODO: There is no quickly achievable std alternative so go with gaia container
#include "impl/sarray_ext_impl.h"

namespace gaia {
	namespace cnt {
		template <typename T, uint32_t N>
		using sarray_ext = cnt::sarr_ext<T, N>;
	} // namespace cnt
} // namespace gaia
