#pragma once

#include "gaia/cnt/impl/sarray_impl.h"

namespace gaia {
	namespace cnt {
		//! Fixed-size contiguous array stored inline.
		//! \tparam T Element type.
		//! \tparam N Number of elements and fixed capacity.
		template <typename T, sarr_detail::size_type N>
		using sarray = cnt::sarr<T, N>;
	} // namespace cnt
} // namespace gaia
