#pragma once

#include "impl/sarray_impl.h"

namespace gaia {
	namespace containers {
		template <typename T, size_t N>
		using sarray = containers::sarr<T, N>;
	} // namespace containers
} // namespace gaia
