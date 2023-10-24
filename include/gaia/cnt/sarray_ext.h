#pragma once

#include "impl/sarray_ext_impl.h"

namespace gaia {
	namespace cnt {
		template <typename T, sarr_ext_detail::size_type N>
		using sarray_ext = cnt::sarr_ext<T, N>;
	} // namespace cnt
} // namespace gaia
