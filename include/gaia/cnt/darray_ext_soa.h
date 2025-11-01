#pragma once

#include "impl/darray_ext_soa_impl.h"

namespace gaia {
	namespace cnt {
		template <typename T, darr_ext_soa_detail::size_type N>
		using darray_ext_soa = cnt::darr_ext_soa<T, N>;
	} // namespace cnt
} // namespace gaia
