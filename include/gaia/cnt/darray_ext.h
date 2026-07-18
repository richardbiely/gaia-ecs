#pragma once

#include "gaia/cnt/impl/darray_ext_impl.h"

namespace gaia {
	namespace cnt {
		//! Dynamically sized contiguous array with embedded storage for the first N elements.
		//! \tparam T Element type.
		//! \tparam N Number of elements accommodated by the embedded storage.
		template <typename T, darr_ext_detail::size_type N>
		using darray_ext = cnt::darr_ext<T, N>;
	} // namespace cnt
} // namespace gaia
