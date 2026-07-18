#pragma once

#include "gaia/cnt/impl/sarray_soa_impl.h"

namespace gaia {
	namespace cnt {
		//! Fixed-size structure-of-arrays container stored inline.
		//! \tparam T Element type described by Gaia-ECS structure-of-arrays reflection metadata.
		//! \tparam N Number of elements and fixed capacity.
		template <typename T, sarr_soa_detail::size_type N>
		using sarray_soa = cnt::sarr_soa<T, N>;
	} // namespace cnt
} // namespace gaia
