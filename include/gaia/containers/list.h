#pragma once

#define USE_LIST 1 // GAIA_USE_STL_CONTAINERS
// TODO: Implement a custom list container

#if USE_LIST == 1

	#include <list>

namespace gaia {
	namespace containers {
		template <typename T>
		using list = std::list<T>;
	} // namespace containers
} // namespace gaia
#else

	#error Unsupported value used for USE_LIST

#endif
