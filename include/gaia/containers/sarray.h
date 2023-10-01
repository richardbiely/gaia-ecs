#pragma once

#include "impl/sarray_impl.h"

namespace gaia {
	namespace containers {
		template <typename T, uint32_t N>
		using sarray = containers::sarr<T, N>;
	} // namespace containers
} // namespace gaia
