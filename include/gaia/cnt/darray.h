#pragma once

#include "gaia/cnt/impl/darray_impl.h"

namespace gaia {
	namespace cnt {
		//! Dynamically sized contiguous array.
		//! \tparam T Element type.
		template <typename T>
		using darray = cnt::darr<T>;
	} // namespace cnt
} // namespace gaia
