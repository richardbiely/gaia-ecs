#pragma once

#include "impl/sarray_impl.h"

namespace gaia {
	namespace cnt {
		template <typename T, uint32_t N>
		using sarray = cnt::sarr<T, N>;
	} // namespace cnt
} // namespace gaia
