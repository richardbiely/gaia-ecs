#pragma once

#include "gaia/cnt/impl/darray_ext_soa_impl.h"

namespace gaia {
	namespace cnt {
		//! Dynamically sized structure-of-arrays container with embedded storage for the first N elements.
		//! \tparam T Element type described by Gaia-ECS structure-of-arrays reflection metadata.
		//! \tparam N Number of elements accommodated by the embedded storage.
		template <typename T, darr_ext_soa_detail::size_type N>
		using darray_ext_soa = cnt::darr_ext_soa<T, N>;
	} // namespace cnt
} // namespace gaia
