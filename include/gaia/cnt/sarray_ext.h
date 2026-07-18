#pragma once

#include "gaia/cnt/impl/sarray_ext_impl.h"

namespace gaia {
	namespace cnt {
		//! Fixed-capacity contiguous array with variable size and inline storage.
		//! \tparam T Element type.
		//! \tparam N Maximum number of elements.
		template <typename T, sarr_ext_detail::size_type N>
		using sarray_ext = cnt::sarr_ext<T, N>;
	} // namespace cnt
} // namespace gaia
