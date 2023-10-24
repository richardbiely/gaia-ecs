#pragma once

#include "impl/darray_ext_impl.h"

namespace gaia {
	namespace cnt {
		template <typename T, darr_ext_detail::size_type N>
		using darray_ext = cnt::darr_ext<T, N>;
	} // namespace cnt
} // namespace gaia
