#pragma once

#include "gaia/cnt/impl/sarray_impl.h"

namespace gaia {
	namespace cnt {
		template <typename T, sarr_detail::size_type N>
		using sarray = cnt::sarr<T, N>;
	} // namespace cnt
} // namespace gaia
