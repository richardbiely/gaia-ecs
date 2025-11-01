#pragma once

#include "impl/sarray_ext_soa_impl.h"

namespace gaia {
	namespace cnt {
		template <typename T, sarr_ext_soa_detail::size_type N>
		using sarray_ext_soa = cnt::sarr_ext_soa<T, N>;
	} // namespace cnt
} // namespace gaia
