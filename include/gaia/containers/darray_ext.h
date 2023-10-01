#pragma once

// TODO: There is no quickly achievable std alternative so go with gaia container
#include "impl/darray_ext_impl.h"

namespace gaia {
	namespace containers {
		template <typename T, uint32_t N>
		using darray_ext = containers::darr_ext<T, N>;
	} // namespace containers
} // namespace gaia
