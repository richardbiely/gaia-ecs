#pragma once

#include "gaia/cnt/impl/sarray_ext_soa_impl.h"

namespace gaia {
	namespace cnt {
		//! Fixed-capacity structure-of-arrays container with variable size and inline storage.
		//! \tparam T Element type described by Gaia-ECS structure-of-arrays reflection metadata.
		//! \tparam N Maximum number of elements.
		template <typename T, sarr_ext_soa_detail::size_type N>
		using sarray_ext_soa = cnt::sarr_ext_soa<T, N>;
	} // namespace cnt
} // namespace gaia
