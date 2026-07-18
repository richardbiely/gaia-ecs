#pragma once

#include "gaia/cnt/impl/darray_soa_impl.h"

namespace gaia {
	namespace cnt {
		//! Dynamically sized structure-of-arrays container.
		//! \tparam T Element type described by Gaia-ECS structure-of-arrays reflection metadata.
		template <typename T>
		using darray_soa = cnt::darr_soa<T>;
	} // namespace cnt
} // namespace gaia
